# InsightFace Rust Library

A high-performance Rust library for face detection and recognition using ONNX models, replicating the functionality of the Python InsightFace project.

## ✨ Features

- 🎯 **Face Detection** - RetinaFace-based detection with high accuracy
- 🔄 **EXIF Orientation Support** - Automatic handling of rotated images from smartphones
- 🎨 **Bounding Box Visualization** - Draw detection results on images
- ⚡ **High Performance** - Optimized with ONNX Runtime
- 🛠️ **Easy to Use** - Simple API accepting both image objects and file paths
- 📦 **Standalone** - No Python dependencies required

## 🚀 Quick Start

### Basic Usage - From Image Object

```rust
use insightface_rs::FaceAnalysis;
use std::path::Path;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Initialize with models directory
    let mut app = FaceAnalysis::new(
        Path::new("~/.insightface/models/buffalo_l"),
        None,
    )?;

    // Prepare: ctx_id, detection_threshold, input_size
    app.prepare(0, 0.5, (640, 640))?;

    // Detect faces from image object
    let img = image::open("photo.jpg")?;
    let faces = app.get(&img)?;

    println!("Found {} faces", faces.len());
    for face in &faces {
        println!("  - Confidence: {:.2}", face.det_score);
        println!("    Box: [{:.0}, {:.0}, {:.0}, {:.0}]",
            face.bbox[0], face.bbox[1], face.bbox[2], face.bbox[3]);
    }

    Ok(())
}
```

### From File Path (with automatic EXIF orientation)

```rust
use insightface_rs::FaceAnalysis;
use std::path::Path;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut app = FaceAnalysis::new(
        Path::new("~/.insightface/models/buffalo_l"),
        None,
    )?;
    app.prepare(0, 0.5, (640, 640))?;

    // Detect faces from file path - handles EXIF rotation automatically
    let faces = app.get_from_path("IMG_1234.jpg")?;
    
    // Draw and save results
    let img = image::open("IMG_1234.jpg")?;
    let result = app.draw_on(&img, &faces);
    result.save("output.jpg")?;

    Ok(())
}
```

### Using as a Library Dependency

Add to your `Cargo.toml`:

```toml
[dependencies]
insightface-rs = { path = "/path/to/insightface_rs" }
# or when published:
# insightface-rs = "0.1.0"
```

## 📋 Requirements

- Rust 1.70+
- ONNX Runtime (system installed)
- ONNX models in specified directory (e.g., `~/.insightface/models/buffalo_l/`)

### Installazione ONNX Runtime

#### macOS (con Homebrew)
```bash
brew install onnxruntime
```

#### Linux (Ubuntu/Debian)
```bash
# Scarica da https://github.com/microsoft/onnxruntime/releases
# oppure compila dal sorgente
```

#### Configurazione alternativa

Se ONNX Runtime è in una posizione non standard, modifica `.cargo/config.toml`:
```toml
[env]
ORT_STRATEGY = "system"
ORT_LIB_LOCATION = "/path/to/onnxruntime/lib"
```

### Download dei modelli

### Download dei modelli

I modelli devono essere collocati in una directory accessibile. L'approccio consigliato:

#### Scarica i modelli dal progetto Python

```bash
# Installa Python InsightFace
pip install insightface

# Scarica i modelli (verranno salvati in ~/.insightface/models/)
python -c "from insightface.app import FaceAnalysis; app = FaceAnalysis('buffalo_l')"
```

Puoi usare i modelli in due modi:

#### 1. **Dalla directory Python** (Consigliato)

La libreria cerca automaticamente in `~/.insightface/models/buffalo_l/`:

```rust
let models_dir = Path::new(&std::env::var("HOME")?).join(".insightface/models/buffalo_l");
let mut app = FaceAnalysis::new(&models_dir, None)?;
```

#### 2. **Da una directory personalizzata**

```bash
# Copia i modelli dove preferisci
mkdir -p ./models/buffalo_l
cp ~/.insightface/models/buffalo_l/*.onnx ./models/buffalo_l/
```

```rust
let mut app = FaceAnalysis::new(Path::new("./models/buffalo_l"), None)?; 
```rust
let mut app = FaceAnalysis::new(Path::new("./models/buffalo_l"), None)?;
```

Oppure scarica i modelli manualmente da [InsightFace Model Zoo](https://github.com/deepinsight/insightface/wiki/Model-Zoo).

I modelli necessari sono:
- `retinaface_resnet50_batch.onnx` o `detection_scrfd_*.onnx` - Rilevamento volti
- `arcface_w600k_r50.onnx` - Riconoscimento volti (opzionale)
- `genderage.onnx` - Stima genere ed età (opzionale)

## Installazione

Aggiungi la dipendenza al tuo `Cargo.toml`:

```toml
[dependencies]
insightface-rs = { path = "../path/to/insightface_rs" }
image = "0.25"
```

## Utilizzo

### Esempio base

```rust
use insightface_rs::FaceAnalysis;
use std::path::Path;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Specifica la directory dei modelli
    let models_dir = Path::new(&std::env::var("HOME")?)
        .join(".insightface/models/buffalo_l");
    
    // Inizializza FaceAnalysis
    let mut app = FaceAnalysis::new(&models_dir, None)?;
    
    // Prepara il modello
    app.prepare(0, 0.5, (640, 640))?;
    
    // Carica e elabora un'immagine
    let img = image::open("face.jpg")?;
    let faces = app.get(&img)?;
    
    // Elabora i risultati
    for (i, face) in faces.iter().enumerate() {
        println!("Face {}: bbox={:?}, score={:.3}", i, face.bbox, face.det_score);
        
        // Accedi ai keypoints se disponibili
        if let Some(kps) = &face.kps {
            for (j, kp) in kps.iter().enumerate() {
                println!("  Landmark {}: x={:.1}, y={:.1}", j, kp[0], kp[1]);
            }
        }
    }
    
    Ok(())
}
    
    Ok(())
}
```

### Con modelli da percorso esterno (fallback)

Se i modelli non sono embedded, puoi anche usare:

```rust
use insightface_rs::FaceAnalysis;
use std::path::Path;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Fallback a modelli da ~/.insightface/models/
    let mut app = FaceAnalysis::new(Path::new("ignored"), None)?;
    app.prepare(0, 0.5, (640, 640))?;
    
    // Resto del codice...
    Ok(())
}
```
    }
    
    Ok(())
}
```

### Configurazione

```rust
// Cambia la soglia di rilevamento
app.set_det_thresh(0.6);

// Cambia la soglia NMS
app.set_nms_thresh(0.3);

// Cambia la dimensione dell'input
app.prepare(0, 0.5, (512, 512))?;
```

## Struttura della libreria

- `app.rs` - API principale (`FaceAnalysis`)
- `face.rs` - Struttura `Face` e utilities
- `error.rs` - Tipi di errore
- `model_zoo.rs` - Gestione dei modelli ONNX
- `utils.rs` - Funzioni di utility (NMS, embedding distance, etc.)

## Struttura dei risultati

Ogni `Face` contiene:

```rust
pub struct Face {
    pub bbox: [f32; 4],              // [x1, y1, x2, y2]
    pub det_score: f32,              // Confidence score (0-1)
    pub kps: Option<Vec<[f32; 2]>>, // 5 Keypoints facoltativi
    pub embedding: Option<Vec<f32>>, // Face embedding (futuro)
    pub gender: Option<i32>,         // 0=Female, 1=Male (futuro)
    pub age: Option<i32>,            // Età stimata (futuro)
    pub attributes: Option<HashMap<String, serde_json::Value>>,
}
```

## Metodi di Face

```rust
// Dimensioni bounding box
face.width()              // Larghezza
face.height()             // Altezza
face.area()               // Area

// Embedding (se disponibile)
face.embedding_norm()     // Norma L2 dell'embedding
face.normed_embedding()   // Embedding normalizzato (unit vector)

// Genere
face.sex()                // "M" o "F"
```

## Utility functions

```rust
use insightface_rs::utils::*;

// NMS - Non-Maximum Suppression
let dets = [[x1, y1, x2, y2, score], ...];
let keep = nms(&dets, 0.4);  // Soglia IoU

// Distanza embedding (L2)
let distance = embedding_distance(&emb1, &emb2);

// Similarità coseno
let similarity = cosine_similarity(&emb1, &emb2);
```

## Esempi

### Processare una directory di immagini

```rust
use std::fs;
use insightface_rs::FaceAnalysis;
use std::path::Path;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let model_dir = Path::new("/Users/alessandro/.insightface/models");
    let mut app = FaceAnalysis::new(model_dir, None)?;
    app.prepare(0, 0.5, (640, 640))?;
    
    // Processa tutte le immagini in una directory
    for entry in fs::read_dir("./images")? {
        let entry = entry?;
        let path = entry.path();
        
        if path.extension().map(|e| e == "jpg" || e == "png").unwrap_or(false) {
            let img = image::open(&path)?;
            let faces = app.get(&img)?;
            
            println!("{}: {} faces detected", path.display(), faces.len());
        }
    }
    
    Ok(())
}
```

## Confronto con la versione Python

| Feature                | Rust | Python |
| ---------------------- | ---- | ------ |
| Rilevamento            | ✅    | ✅      |
| Embedding              | 🚧    | ✅      |
| Attributi (age/gender) | 🚧    | ✅      |
| Visualizzazione        | -    | ✅      |
| Performance            | ⚡    | ⚡      |

## Nota sulla performance

La libreria Rust è generalmente più veloce della versione Python per le operazioni core, poiché:
- Non ha overhead di interpretazione Python
- I tensor sono allocati in memoria Rust nativa
- L'inferenza ONNX è la stessa in entrambi i casi

## Roadmap

- [ ] Supporto embedding ArcFace
- [ ] Riconoscimento attributi (age/gender)
- [ ] Supporto GPU (CUDA/TensorRT)
- [ ] Batch processing
- [ ] Binding Python per la libreria Rust

## Licenza

Questo progetto segue la licenza del progetto InsightFace originale.

## Riferimenti

- [InsightFace GitHub](https://github.com/deepinsight/insightface)
- [ONNX Runtime Rust](https://github.com/nbz0/onnxruntime-rs)
- [Documentazione RetinaFace](https://github.com/biubug6/Pytorch_Retinaface)
