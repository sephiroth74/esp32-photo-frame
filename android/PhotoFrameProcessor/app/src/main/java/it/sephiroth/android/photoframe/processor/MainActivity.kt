package it.sephiroth.android.photoframe.processor

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.BitmapFactory
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.MediaStore
import android.util.Log
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.ImageView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.ViewModelProvider
import androidx.recyclerview.widget.GridLayoutManager
import com.bumptech.glide.Glide
import it.sephiroth.android.photoframe.processor.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {
    
    companion object {
        const val TAG = "MainActivity"
    }
    
    private lateinit var binding: ActivityMainBinding
    private lateinit var viewModel: MainViewModel
    private lateinit var imageAdapter: ImageAdapter
    private var processMenuItem: MenuItem? = null
    private var shareMenuItem: MenuItem? = null
    private var clearAllMenuItem: MenuItem? = null
    
    private val pickImagesLauncher = registerForActivityResult(
        ActivityResultContracts.GetMultipleContents()
    ) { uris ->
        Log.d(TAG, "pickImagesLauncher: Received ${uris.size} images from picker")
        if (uris.isNotEmpty()) {
            // Show loading overlay with fade in animation during analysis
            binding.analysisProgressText.text = "Analyzing ${uris.size} images..."
            binding.analysisLoadingOverlay.alpha = 0f
            binding.analysisLoadingOverlay.visibility = View.VISIBLE
            binding.analysisLoadingOverlay.animate()
                .alpha(1f)
                .setDuration(200)
                .start()
            
            viewModel.addImages(uris)
            updateUI()
        } else {
            Log.d(TAG, "pickImagesLauncher: No images selected")
        }
    }
    
    private val requestMultiplePermissionsLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        val allGranted = permissions.values.all { it }
        val grantedCount = permissions.values.count { it }
        Log.d(TAG, "requestMultiplePermissionsLauncher: $grantedCount/${permissions.size} permissions granted")
        
        if (allGranted) {
            Log.d(TAG, "requestMultiplePermissionsLauncher: All permissions granted, opening image picker")
            openImagePicker()
        } else {
            val deniedPermissions = permissions.filterValues { !it }.keys
            Log.w(TAG, "requestMultiplePermissionsLauncher: Denied permissions: $deniedPermissions")
            showPermissionDeniedMessage(deniedPermissions)
        }
    }
    
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Log.i(TAG, "onCreate: Initializing MainActivity")
        
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        
        // Set up the custom toolbar
        setSupportActionBar(binding.toolbar)
        
        viewModel = ViewModelProvider(this)[MainViewModel::class.java]
        Log.d(TAG, "onCreate: ViewModel initialized")
        
        setupRecyclerView()
        setupButtons()
        observeViewModel()
        Log.d(TAG, "onCreate: UI setup completed")
        
        // Handle shared images
        handleSharedImages(intent)
        Log.i(TAG, "onCreate: MainActivity initialization completed")
    }
    
    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        Log.d(TAG, "onNewIntent: Received new intent")
        handleSharedImages(intent)
    }
    
    private fun setupRecyclerView() {
        imageAdapter = ImageAdapter(
            onRemoveClick = { uri ->
                viewModel.removeImage(uri)
                updateUI()
            },
            onLongPressPreview = { processedImage ->
                showFullPreview(processedImage)
            }
        )
        binding.imagesRecyclerView.apply {
            layoutManager = GridLayoutManager(this@MainActivity, 2) // 2 columns
            adapter = imageAdapter
        }
    }
    
    private fun setupButtons() {
        Log.d(TAG, "setupButtons: Setting up button click listeners")
        
        binding.pickImagesFab.setOnClickListener {
            Log.d(TAG, "pickImagesFab: Button clicked")
            if (checkRequiredPermissions()) {
                Log.d(TAG, "pickImagesFab: Permissions granted, opening image picker")
                openImagePicker()
            } else {
                Log.d(TAG, "pickImagesFab: Permissions not granted, requesting permissions")
                requestRequiredPermissions()
            }
        }
    }
    
    private fun observeViewModel() {
        viewModel.selectedImages.observe(this) { images ->
            imageAdapter.updateImages(images)
            
            // Hide loading overlay with fade out animation when images are ready
            if (binding.analysisLoadingOverlay.visibility == View.VISIBLE) {
                binding.analysisLoadingOverlay.animate()
                    .alpha(0f)
                    .setDuration(300)
                    .withEndAction {
                        binding.analysisLoadingOverlay.visibility = View.GONE
                        binding.analysisLoadingOverlay.alpha = 1f // Reset for next time
                    }
                    .start()
            }
            
            updateUI()
        }
        
        viewModel.isProcessing.observe(this) { isProcessing ->
            updateProgressBarAndFab()
            updateProcessButtonState()
        }
        
        viewModel.isImporting.observe(this) { isImporting ->
            updateProgressBarAndFab()
        }
        
        // No need for separate allImagesValid or hasValidImages observers
        // Process button state is computed directly from selectedImages
        
        viewModel.processedFiles.observe(this) { files ->
            shareMenuItem?.isEnabled = files.isNotEmpty()
        }
        
        viewModel.statusMessage.observe(this) { message ->
            binding.statusText.text = message
            if (message.contains("error", ignoreCase = true)) {
                Toast.makeText(this, message, Toast.LENGTH_LONG).show()
            }
        }
        
        viewModel.shareStatisticsMessage.observe(this) { message ->
            if (message.isNotEmpty()) {
                com.google.android.material.snackbar.Snackbar.make(
                    binding.root,
                    message,
                    com.google.android.material.snackbar.Snackbar.LENGTH_LONG
                ).show()
            }
        }
    }
    
    private fun updateProgressBarAndFab() {
        val isProcessing = viewModel.isProcessing.value == true
        val isImporting = viewModel.isImporting.value == true
        val isBusy = isProcessing || isImporting
        
        // Progress is now handled by individual shimmer effects on images
        binding.pickImagesFab.isEnabled = !isBusy
    }
    
    private fun updateProcessButtonState() {
        val images = viewModel.selectedImages.value
        val hasImages = images?.isNotEmpty() == true
        val isProcessing = viewModel.isProcessing.value == true
        val isImporting = viewModel.isImporting.value == true
        val isBusy = isProcessing || isImporting
        
        // Compute hasValidImages directly from the current images list
        val hasValidImages = images?.any { !it.isInvalid } == true
        val shouldEnable = hasImages && !isBusy && hasValidImages
        
        if (processMenuItem == null) {
            invalidateOptionsMenu()
        } else {
            processMenuItem?.isEnabled = shouldEnable
        }
    }
    
    private fun updateClearAllButtonState() {
        val hasImages = viewModel.selectedImages.value?.isNotEmpty() == true
        clearAllMenuItem?.isEnabled = hasImages
    }
    
    private fun updateUI() {
        updateProcessButtonState()
        updateClearAllButtonState()
        
        val statusText = when {
            viewModel.selectedImages.value?.isEmpty() != false -> "Select images to process"
            viewModel.processedFiles.value?.isNotEmpty() == true -> "Ready"
            else -> "Ready to process"
        }
        binding.statusText.text = statusText
    }
    
    private fun handleSharedImages(intent: Intent) {
        Log.d(TAG, "handleSharedImages: Processing intent action: ${intent.action}")
        
        when (intent.action) {
            Intent.ACTION_SEND -> {
                intent.getParcelableExtra<Uri>(Intent.EXTRA_STREAM)?.let { uri ->
                    Log.i(TAG, "handleSharedImages: Received single shared image: $uri")
                    viewModel.addImages(listOf(uri))
                } ?: Log.d(TAG, "handleSharedImages: ACTION_SEND but no URI found")
            }
            Intent.ACTION_SEND_MULTIPLE -> {
                intent.getParcelableArrayListExtra<Uri>(Intent.EXTRA_STREAM)?.let { uris ->
                    Log.i(TAG, "handleSharedImages: Received ${uris.size} shared images")
                    viewModel.addImages(uris)
                } ?: Log.d(TAG, "handleSharedImages: ACTION_SEND_MULTIPLE but no URIs found")
            }
            else -> {
                if (intent.action != null) {
                    Log.d(TAG, "handleSharedImages: Unhandled intent action: ${intent.action}")
                }
            }
        }
    }
    
    private fun getRequiredPermissions(): Array<String> {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            // Android 13+ uses granular media permissions
            arrayOf(Manifest.permission.READ_MEDIA_IMAGES)
        } else {
            // Android 12 and below use legacy storage permissions
            arrayOf(
                Manifest.permission.READ_EXTERNAL_STORAGE,
                Manifest.permission.WRITE_EXTERNAL_STORAGE
            )
        }
    }
    
    private fun checkRequiredPermissions(): Boolean {
        val requiredPermissions = getRequiredPermissions()
        return requiredPermissions.all { permission ->
            ContextCompat.checkSelfPermission(this, permission) == PackageManager.PERMISSION_GRANTED
        }
    }
    
    private fun requestRequiredPermissions() {
        val requiredPermissions = getRequiredPermissions()
        val permissionsToRequest = requiredPermissions.filter { permission ->
            ContextCompat.checkSelfPermission(this, permission) != PackageManager.PERMISSION_GRANTED
        }.toTypedArray()
        
        if (permissionsToRequest.isNotEmpty()) {
            // Check if we should show rationale for any of the permissions
            val shouldShowRationale = permissionsToRequest.any { permission ->
                ActivityCompat.shouldShowRequestPermissionRationale(this, permission)
            }
            
            if (shouldShowRationale) {
                showPermissionRationale(permissionsToRequest)
            } else {
                requestMultiplePermissionsLauncher.launch(permissionsToRequest)
            }
        }
    }
    
    private fun showPermissionRationale(permissions: Array<String>) {
        val message = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            "This app needs access to your photos to process and upload them to Google Drive."
        } else {
            "This app needs storage permissions to read your photos and save processed images."
        }
        
        androidx.appcompat.app.AlertDialog.Builder(this)
            .setTitle("Permission Required")
            .setMessage(message)
            .setPositiveButton("Grant Permission") { _, _ ->
                requestMultiplePermissionsLauncher.launch(permissions)
            }
            .setNegativeButton("Cancel") { dialog, _ ->
                dialog.dismiss()
                Toast.makeText(this, "Permission required to access images", Toast.LENGTH_SHORT).show()
            }
            .show()
    }
    
    private fun showPermissionDeniedMessage(deniedPermissions: Set<String>) {
        val message = when {
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU && 
            deniedPermissions.contains(Manifest.permission.READ_MEDIA_IMAGES) -> {
                "Photo access permission is required to select and process images. Please enable it in Settings."
            }
            deniedPermissions.contains(Manifest.permission.READ_EXTERNAL_STORAGE) -> {
                "Storage permission is required to access your photos. Please enable it in Settings."
            }
            else -> "Required permissions were denied. Please enable them in Settings."
        }
        
        androidx.appcompat.app.AlertDialog.Builder(this)
            .setTitle("Permission Denied")
            .setMessage(message)
            .setPositiveButton("Open Settings") { _, _ ->
                openAppSettings()
            }
            .setNegativeButton("Cancel") { dialog, _ ->
                dialog.dismiss()
            }
            .show()
    }
    
    private fun openAppSettings() {
        val intent = Intent(android.provider.Settings.ACTION_APPLICATION_DETAILS_SETTINGS).apply {
            data = Uri.fromParts("package", packageName, null)
        }
        startActivity(intent)
    }
    
    private fun openImagePicker() {
        Log.d(TAG, "openImagePicker: Launching image picker")
        pickImagesLauncher.launch("image/*")
    }
    
    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.main_menu, menu)
        processMenuItem = menu.findItem(R.id.action_process)
        shareMenuItem = menu.findItem(R.id.action_share)
        clearAllMenuItem = menu.findItem(R.id.action_clear_all)
        
        // Update initial states
        updateProcessButtonState()
        shareMenuItem?.isEnabled = viewModel.processedFiles.value?.isNotEmpty() == true
        updateClearAllButtonState()
        
        return true
    }
    
    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.action_process -> {
                Log.i(TAG, "action_process: Menu item clicked - starting image processing")
                viewModel.processImages()
                true
            }
            R.id.action_share -> {
                Log.i(TAG, "action_share: Menu item clicked - showing share options")
                showShareOptionsDialog()
                true
            }
            R.id.action_clear_all -> {
                Log.i(TAG, "action_clear_all: Menu item clicked - clearing all images")
                viewModel.clearAll()
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }
    
    private fun showFullPreview(processedImage: ProcessedImage) {
        Log.d(TAG, "showFullPreview: Showing fullscreen preview for ${processedImage.originalUri}")
        
        val previewFile = processedImage.previewFile
        if (previewFile == null || !previewFile.exists()) {
            Log.w(TAG, "showFullPreview: Preview file not found - ${previewFile?.absolutePath}")
            Toast.makeText(this, "Preview not available", Toast.LENGTH_SHORT).show()
            return
        }
        
        Log.d(TAG, "showFullPreview: Loading preview from ${previewFile.absolutePath} (${previewFile.length()} bytes)")
        
        // Create fullscreen overlay
        val overlay = layoutInflater.inflate(R.layout.fullscreen_image_overlay, null)
        val imageView = overlay.findViewById<ImageView>(R.id.fullscreenImageView)
        val closeButton = overlay.findViewById<View>(R.id.closeButton)
        
        // Add overlay to root view
        val rootView = findViewById<android.view.ViewGroup>(android.R.id.content)
        rootView.addView(overlay)
        
        // Set up close functionality - tap anywhere to close
        overlay.setOnClickListener {
            Log.d(TAG, "showFullPreview: Closing fullscreen preview")
            rootView.removeView(overlay)
        }
        
        closeButton.setOnClickListener {
            Log.d(TAG, "showFullPreview: Close button clicked")
            rootView.removeView(overlay)
        }
        
        // Load the full preview image with error handling
        Glide.with(this)
            .load(previewFile)
            .skipMemoryCache(true)
            .listener(object : com.bumptech.glide.request.RequestListener<android.graphics.drawable.Drawable> {
                override fun onLoadFailed(
                    e: com.bumptech.glide.load.engine.GlideException?, 
                    model: Any?, 
                    target: com.bumptech.glide.request.target.Target<android.graphics.drawable.Drawable>?, 
                    isFirstResource: Boolean
                ): Boolean {
                    Log.e(TAG, "showFullPreview: Failed to load image: ${e?.message}")
                    runOnUiThread {
                        Toast.makeText(this@MainActivity, "Failed to load preview", Toast.LENGTH_SHORT).show()
                        rootView.removeView(overlay)
                    }
                    return false
                }
                
                override fun onResourceReady(
                    resource: android.graphics.drawable.Drawable?, 
                    model: Any?, 
                    target: com.bumptech.glide.request.target.Target<android.graphics.drawable.Drawable>?, 
                    dataSource: com.bumptech.glide.load.DataSource?, 
                    isFirstResource: Boolean
                ): Boolean {
                    Log.d(TAG, "showFullPreview: Image loaded successfully")
                    return false
                }
            })
            .into(imageView)
        
        Log.d(TAG, "showFullPreview: Fullscreen overlay shown with preview from ${previewFile.name}")
    }
    
    private fun showShareOptionsDialog() {
        Log.d(TAG, "showShareOptionsDialog: Showing share options dialog")
        
        val options = arrayOf("Binary files (.bin)", "Preview images (.bmp)", "Both (.bin and .bmp)")
        
        android.app.AlertDialog.Builder(this)
            .setTitle("What would you like to share?")
            .setItems(options) { _, which ->
                when (which) {
                    0 -> {
                        Log.i(TAG, "showShareOptionsDialog: User selected binary files")
                        viewModel.shareProcessedFiles(shareType = "binary")
                    }
                    1 -> {
                        Log.i(TAG, "showShareOptionsDialog: User selected preview images")
                        viewModel.shareProcessedFiles(shareType = "preview")
                    }
                    2 -> {
                        Log.i(TAG, "showShareOptionsDialog: User selected both file types")
                        viewModel.shareProcessedFiles(shareType = "both")
                    }
                }
            }
            .setNegativeButton("Cancel") { dialog, _ -> 
                Log.d(TAG, "showShareOptionsDialog: User cancelled share dialog")
                dialog.dismiss() 
            }
            .show()
    }
    
    override fun onDestroy() {
        Log.d(TAG, "onDestroy: Cleaning up MainActivity")
        super.onDestroy()
        imageAdapter.cleanup()
        Log.d(TAG, "onDestroy: MainActivity cleanup completed")
    }
}