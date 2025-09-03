package it.sephiroth.android.photoframe.processor

import android.net.Uri
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.RecyclerView
import com.bumptech.glide.Glide
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class ImageAdapter(
    private val onRemoveClick: (Uri) -> Unit,
    private val onLongPressPreview: (ProcessedImage) -> Unit
) : RecyclerView.Adapter<ImageAdapter.ImageViewHolder>() {
    
    companion object {
        const val TAG = "ImageAdapter"
    }
    
    private var images = listOf<ProcessedImage>()
    private val adapterScope = CoroutineScope(Dispatchers.Main + SupervisorJob())
    private var currentUpdateJob: Job? = null
    
    fun cleanup() {
        Log.d(TAG, "cleanup: Cancelling adapter coroutine scope and current update job")
        currentUpdateJob?.cancel()
        adapterScope.cancel()
    }
    
    fun updateImages(newImages: List<ProcessedImage>) {
        // Cancel any pending update to prevent race conditions
        currentUpdateJob?.cancel()
        
        val oldImages = images
        
        Log.d(TAG, "updateImages: Updating from ${oldImages.size} to ${newImages.size} images")
        
        if (oldImages.isEmpty() && newImages.isNotEmpty()) {
            Log.d(TAG, "updateImages: Initial load of ${newImages.size} images")
        } else if (newImages.isEmpty() && oldImages.isNotEmpty()) {
            Log.d(TAG, "updateImages: Clearing all ${oldImages.size} images")
        }
        
        // Calculate diff on background thread to avoid blocking main thread
        currentUpdateJob = adapterScope.launch {
            val diffResult = withContext(Dispatchers.Default) {
                val diffCallback = ImageDiffCallback(oldImages, newImages)
                DiffUtil.calculateDiff(diffCallback)
            }
            // Update the data and apply changes on main thread atomically
            images = newImages
            Log.v(TAG, "updateImages: Applying diff updates to RecyclerView")
            diffResult.dispatchUpdatesTo(this@ImageAdapter)
        }
    }
    
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ImageViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.item_image, parent, false)
        return ImageViewHolder(view)
    }
    
    override fun onBindViewHolder(holder: ImageViewHolder, position: Int) {
        if (position >= 0 && position < images.size) {
            holder.bind(images[position])
        } else {
            Log.w(TAG, "onBindViewHolder: Invalid position $position for ${images.size} images")
        }
    }
    
    override fun getItemCount() = images.size
    
    inner class ImageViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val imageView: ImageView = itemView.findViewById(R.id.imageView)
        private val pairedImagesContainer: LinearLayout = itemView.findViewById(R.id.pairedImagesContainer)
        private val firstPortraitImage: ImageView = itemView.findViewById(R.id.firstPortraitImage)
        private val secondPortraitImage: ImageView = itemView.findViewById(R.id.secondPortraitImage)
        private val removeButton: ImageButton = itemView.findViewById(R.id.removeButton)
        private val missingPortraitOverlay: View = itemView.findViewById(R.id.missingPortraitOverlay)
        private val missingPortraitText: TextView = itemView.findViewById(R.id.missingPortraitText)
        private val pairLabel: TextView = itemView.findViewById(R.id.pairLabel)
        
        fun bind(processedImage: ProcessedImage) {
            Log.v(TAG, "bind: Binding image ${processedImage.originalUri} - Invalid: ${processedImage.isInvalid}, Portrait: ${processedImage.isPortrait}, Processing: ${processedImage.processingState}")
            
            // Handle invalid images or missing paired portraits
            if (processedImage.isInvalid || (processedImage.isPortrait && processedImage.pairedWith == null)) {
                // Show single image view
                imageView.visibility = View.VISIBLE
                pairedImagesContainer.visibility = View.GONE
                
                // Show missing portrait overlay and text
                missingPortraitOverlay.visibility = View.VISIBLE
                missingPortraitText.visibility = View.VISIBLE
                pairLabel.visibility = View.GONE
                
                // Show original image with red overlay
                Glide.with(itemView.context)
                    .load(processedImage.originalUri)
                    .centerCrop()
                    .into(imageView)
                    
            } else if (processedImage.isPortrait && processedImage.pairedWith != null) {
                
                if (processedImage.isProcessed && processedImage.previewFile?.exists() == true) {
                    // When portrait images are processed, they become a single landscape result
                    // Show as single image, not paired
                    imageView.visibility = View.VISIBLE
                    pairedImagesContainer.visibility = View.GONE
                    missingPortraitOverlay.visibility = View.GONE
                    missingPortraitText.visibility = View.GONE
                    
                    Glide.with(itemView.context)
                        .load(processedImage.previewFile)
                        .centerCrop()
                        .skipMemoryCache(true)
                        .into(imageView)
                        
                    // Show processed label
                    pairLabel.visibility = View.VISIBLE
                    pairLabel.text = "PROCESSED"
                    
                } else {
                    // Show original paired portrait images side by side
                    imageView.visibility = View.GONE
                    pairedImagesContainer.visibility = View.VISIBLE
                    missingPortraitOverlay.visibility = View.GONE
                    missingPortraitText.visibility = View.GONE
                    
                    Glide.with(itemView.context)
                        .load(processedImage.originalUri)
                        .centerCrop()
                        .into(firstPortraitImage)
                        
                    Glide.with(itemView.context)
                        .load(processedImage.pairedWith.originalUri)
                        .centerCrop()
                        .into(secondPortraitImage)
                        
                    // Show pair label
                    pairLabel.visibility = View.VISIBLE
                    pairLabel.text = "PORTRAITS"
                }
                
            } else {
                // Show single landscape image
                imageView.visibility = View.VISIBLE
                pairedImagesContainer.visibility = View.GONE
                missingPortraitOverlay.visibility = View.GONE
                missingPortraitText.visibility = View.GONE
                
                // Display processed preview if available, otherwise show original image
                if (processedImage.isProcessed && processedImage.previewFile?.exists() == true) {
                    Glide.with(itemView.context)
                        .load(processedImage.previewFile)
                        .centerCrop()
                        .skipMemoryCache(true)
                        .into(imageView)
                } else {
                    Glide.with(itemView.context)
                        .load(processedImage.originalUri)
                        .centerCrop()
                        .into(imageView)
                }
                
                // Show landscape label
                if (!processedImage.isPortrait) {
                    pairLabel.visibility = View.VISIBLE
                    pairLabel.text = "LANDSCAPE"
                } else {
                    pairLabel.visibility = View.GONE
                }
            }
            
            removeButton.setOnClickListener {
                onRemoveClick(processedImage.originalUri)
            }
            
            // Long press to show full preview (only if processed)
            if (processedImage.isProcessed && processedImage.previewFile?.exists() == true) {
                itemView.setOnLongClickListener {
                    onLongPressPreview(processedImage)
                    true
                }
            } else {
                itemView.setOnLongClickListener(null)
            }
        }
        
    }
    
    private class ImageDiffCallback(
        private val oldList: List<ProcessedImage>,
        private val newList: List<ProcessedImage>
    ) : DiffUtil.Callback() {
        
        override fun getOldListSize(): Int = oldList.size
        
        override fun getNewListSize(): Int = newList.size
        
        override fun areItemsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean {
            val oldItem = oldList[oldItemPosition]
            val newItem = newList[newItemPosition]
            return oldItem.originalUri == newItem.originalUri
        }
        
        override fun areContentsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean {
            val oldItem = oldList[oldItemPosition]
            val newItem = newList[newItemPosition]
            
            // Only check the fields that affect the UI to avoid unnecessary updates
            return oldItem.originalUri == newItem.originalUri &&
                    oldItem.isProcessed == newItem.isProcessed &&
                    oldItem.processingState == newItem.processingState &&
                    oldItem.isInvalid == newItem.isInvalid &&
                    oldItem.previewFile?.absolutePath == newItem.previewFile?.absolutePath &&
                    oldItem.isPortrait == newItem.isPortrait &&
                    oldItem.pairedWith?.originalUri == newItem.pairedWith?.originalUri
        }
    }
}