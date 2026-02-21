# URL Encoder/Decoder Tool

A powerful and flexible **URL Encoding and Decoding CLI Tool** designed for developers, security researchers, and penetration testers.

This tool supports multiple encoding formats, smart detection, repeated encoding/decoding, and detailed analysis of encoded strings.

---

## 🚀 Features

- Encode text into URL-safe format
- Decode encoded URLs
- Smart auto-detection decoding
- Analyze encoding type of input
- Multiple encoding formats support
- Repeat encoding/decoding multiple times
- Uppercase or lowercase hex control
- Verbose mode for step-by-step output
- Supports stdin input (pipe support)

---
## 🔧 Options:
| Option               | Description                                                   |
| -------------------- | ------------------------------------------------------------- |
| `-e, --encode TEXT`  | Text to encode (omit TEXT to read from stdin)                 |
| `-d, --decode TEXT`  | Text to decode (omit TEXT to read from stdin)                 |
| `-s, --smart TEXT`   | Smart decode with auto-detection                              |
| `-a, --analyze TEXT` | Analyze encoding type of text                                 |
| `-t, --type TYPE`    | Encoding type: `standard`, `double`, `unicode`, `full`, `all` |
| `--times N`          | Number of times to encode/decode                              |
| `-u, --uppercase`    | Use uppercase hex letters (default: lowercase)                |
| `-v, --verbose`      | Verbose output showing steps                                  |
| `-h, --help`         | Show help message                                             |



## 📦 Installation

### 1️⃣ Requirements

- GCC compiler
- Standard C libraries (included with GCC)
- Linux / macOS (Windows via MinGW)

Check GCC version:

```bash
gcc --version
```

### compilation: 
```bash
gcc -o url main.c
```
### Make Executable (Optional)
```bash
chmod +x urltool
sudo mv urltool /usr/local/bin/
```

## 📄 License

This project is licensed under the MIT License.  
See the [LICENSE](LICENSE) file for details.

---

## 👨‍💻 Author

**Shaikh Khizer**  
Computer Science Student | Penetration Tester

