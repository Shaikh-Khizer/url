#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <wchar.h>
#include <locale.h>
#include <unistd.h>      // For isatty, fileno
#include <sys/time.h>    // For timeval, select
#include <sys/select.h>  // For FD_SET, FD_ZERO

#define MAX_INPUT_LENGTH 65536  // Increased for piping support
#define MAX_OUTPUT_LENGTH (MAX_INPUT_LENGTH * 10)
#define MAX_DECODE_PASSES 20
#define BUFFER_SIZE 4096

// Function prototypes
bool is_hex_digit(char c);
int hex_to_int(char c);
char* url_encode(const char* input, const char* type, int times, bool uppercase);
char* url_decode(const char* input, int times);
char* smart_decode(const char* input, bool verbose);
void analyze_string(const char* input);
char* detect_encoding_type(const char* text);
char* double_encode(const char* input, bool uppercase);
char* unicode_encode(const char* input, bool uppercase);
char* full_encode(const char* input, bool uppercase);
char* decode_unicode_escape(const char* input);
char* to_uppercase_hex(const char* encoded);
bool contains_percent_u(const char* text);
void print_help(void);
bool is_stdin_empty(void);
char* read_stdin(void);
void process_input(const char* input, const char* operation, 
                   const char* type, int times, bool verbose, bool uppercase);

int main(int argc, char* argv[]) {
    // Set locale for wide character support
    setlocale(LC_ALL, "");
    
    // Default values
    char* encode_input = NULL;
    char* decode_input = NULL;
    char* smart_input = NULL;
    char* analyze_input = NULL;
    char* type = "standard";
    int times = 1;
    bool verbose = false;
    bool uppercase = false;
    bool read_from_stdin = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--encode") == 0) {
            if (i + 1 < argc && !strchr(argv[i + 1], '-')) {
                encode_input = argv[++i];
            } else {
                encode_input = "";  // Mark for stdin reading
                read_from_stdin = true;
            }
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--decode") == 0) {
            if (i + 1 < argc && !strchr(argv[i + 1], '-')) {
                decode_input = argv[++i];
            } else {
                decode_input = "";  // Mark for stdin reading
                read_from_stdin = true;
            }
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--smart") == 0) {
            if (i + 1 < argc && !strchr(argv[i + 1], '-')) {
                smart_input = argv[++i];
            } else {
                smart_input = "";  // Mark for stdin reading
                read_from_stdin = true;
            }
        } else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--analyze") == 0) {
            if (i + 1 < argc && !strchr(argv[i + 1], '-')) {
                analyze_input = argv[++i];
            } else {
                analyze_input = "";  // Mark for stdin reading
                read_from_stdin = true;
            }
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--type") == 0) {
            if (i + 1 < argc) {
                type = argv[++i];
            }
        } else if (strcmp(argv[i], "--times") == 0) {
            if (i + 1 < argc) {
                times = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--uppercase") == 0) {
            uppercase = true;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        } else if (strcmp(argv[i], "--") == 0) {
            // End of arguments
            break;
        } else if (argv[i][0] != '-') {
            // This might be input without a flag (for piping)
            if (!encode_input && !decode_input && !smart_input && !analyze_input) {
                // If no operation specified yet, treat as encode
                encode_input = argv[i];
            }
        }
    }
    
    // Check if we should read from stdin (no arguments or explicit stdin markers)
    bool has_explicit_operation = encode_input || decode_input || smart_input || analyze_input;
    
    // If no arguments or we have stdin markers, read from stdin
    if (!has_explicit_operation || read_from_stdin) {
        char* stdin_input = read_stdin();
        if (stdin_input) {
            if (encode_input && encode_input[0] == '\0') {
                process_input(stdin_input, "encode", type, times, verbose, uppercase);
            } else if (decode_input && decode_input[0] == '\0') {
                process_input(stdin_input, "decode", type, times, verbose, uppercase);
            } else if (smart_input && smart_input[0] == '\0') {
                process_input(stdin_input, "smart", type, times, verbose, uppercase);
            } else if (analyze_input && analyze_input[0] == '\0') {
                process_input(stdin_input, "analyze", type, times, verbose, uppercase);
            } else if (!has_explicit_operation) {
                // No operation specified, default to encode
                process_input(stdin_input, "encode", type, times, verbose, uppercase);
            }
            free(stdin_input);
            return 0;
        }
    }
    
    // Check if any operation was specified
    int op_count = (encode_input != NULL) + (decode_input != NULL) + 
                   (smart_input != NULL) + (analyze_input != NULL);
    
    if (op_count == 0) {
        // Check if there's data in stdin
        if (!is_stdin_empty()) {
            char* stdin_input = read_stdin();
            if (stdin_input && strlen(stdin_input) > 0) {
                // Default to encode when piped data
                process_input(stdin_input, "encode", type, times, verbose, uppercase);
                free(stdin_input);
                return 0;
            }
            free(stdin_input);
        }
        
        fprintf(stderr, "Error: No operation specified. Use -e, -d, -s, or -a\n");
        fprintf(stderr, "Use -h for help\n");
        return 1;
    }
    
    if (op_count > 1) {
        fprintf(stderr, "Error: Only one operation can be specified at a time\n");
        return 1;
    }
    
    // Validate times
    if (times < 1) {
        fprintf(stderr, "Error: Times must be at least 1\n");
        return 1;
    }
    
    // Get the actual input string
    char* current_input = NULL;
    char* operation = NULL;
    
    if (encode_input) {
        current_input = (encode_input[0] == '\0') ? read_stdin() : strdup(encode_input);
        operation = "encode";
    } else if (decode_input) {
        current_input = (decode_input[0] == '\0') ? read_stdin() : strdup(decode_input);
        operation = "decode";
    } else if (smart_input) {
        current_input = (smart_input[0] == '\0') ? read_stdin() : strdup(smart_input);
        operation = "smart";
    } else if (analyze_input) {
        current_input = (analyze_input[0] == '\0') ? read_stdin() : strdup(analyze_input);
        operation = "analyze";
    }
    
    if (!current_input) {
        fprintf(stderr, "Error: Failed to get input\n");
        return 1;
    }
    
    // Validate input length
    if (strlen(current_input) > MAX_INPUT_LENGTH) {
        fprintf(stderr, "Error: Input too long (max %d characters)\n", MAX_INPUT_LENGTH);
        free(current_input);
        return 1;
    }
    
    // Process the input
    process_input(current_input, operation, type, times, verbose, uppercase);
    
    // Clean up
    if (current_input != encode_input && current_input != decode_input && 
        current_input != smart_input && current_input != analyze_input) {
        free(current_input);
    }
    
    return 0;
}

void process_input(const char* input, const char* operation, 
                   const char* type, int times, bool verbose, bool uppercase) {
    if (!input || !operation) return;
    
    if (strcmp(operation, "encode") == 0) {
        if (strlen(input) == 0) {
            fprintf(stderr, "Error: Empty string provided for encoding\n");
            return;
        }
        char* result = url_encode(input, type, times, uppercase);
        if (result) {
            const char* case_str = uppercase ? "UPPERCASE" : "lowercase";
            // If output is not a terminal, don't print the prefix
            if (isatty(fileno(stdout))) {
                printf("Encoded (%s x%d, %s): %s\n", type, times, case_str, result);
            } else {
                printf("%s", result);
            }
            free(result);
        } else {
            fprintf(stderr, "Error: Failed to encode string\n");
        }
    } else if (strcmp(operation, "decode") == 0) {
        if (strlen(input) == 0) {
            fprintf(stderr, "Error: Empty string provided for decoding\n");
            return;
        }
        char* result = url_decode(input, times);
        if (result) {
            // If output is not a terminal, don't print the prefix
            if (isatty(fileno(stdout))) {
                printf("Decoded (x%d): %s\n", times, result);
            } else {
                printf("%s", result);
            }
            free(result);
        } else {
            fprintf(stderr, "Error: Failed to decode string\n");
        }
    } else if (strcmp(operation, "smart") == 0) {
        if (strlen(input) == 0) {
            fprintf(stderr, "Error: Empty string provided for smart decoding\n");
            return;
        }
        char* result = smart_decode(input, verbose);
        if (result) {
            // If output is not a terminal, don't print the prefix
            if (isatty(fileno(stdout))) {
                printf("Smart decode result: %s\n", result);
            } else {
                printf("%s", result);
            }
            free(result);
        } else {
            fprintf(stderr, "Error: Failed to smart decode string\n");
        }
    } else if (strcmp(operation, "analyze") == 0) {
        if (strlen(input) == 0) {
            fprintf(stderr, "Error: Empty string provided for analysis\n");
            return;
        }
        // Always show analysis with headers
        analyze_string(input);
    }
}

bool is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || 
           (c >= 'A' && c <= 'F') || 
           (c >= 'a' && c <= 'f');
}

int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

char* url_encode(const char* input, const char* type, int times, bool uppercase) {
    if (!input || times < 1) return NULL;
    
    char* result = strdup(input);
    if (!result) return NULL;
    
    for (int t = 0; t < times; t++) {
        char* temp = NULL;
        
        if (strcmp(type, "standard") == 0) {
            // Standard URL encoding
            int len = strlen(result);
            int new_len = 0;
            
            // Calculate new length
            for (int i = 0; i < len; i++) {
                unsigned char c = result[i];
                if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                    new_len++;
                } else {
                    new_len += 3; // % plus two hex digits
                }
            }
            
            temp = malloc(new_len + 1);
            if (!temp) {
                free(result);
                return NULL;
            }
            
            int pos = 0;
            for (int i = 0; i < len; i++) {
                unsigned char c = result[i];
                if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                    temp[pos++] = c;
                } else {
                    if (uppercase) {
                        snprintf(&temp[pos], 4, "%%%02X", c);
                    } else {
                        snprintf(&temp[pos], 4, "%%%02x", c);
                    }
                    pos += 3;
                }
            }
            temp[pos] = '\0';
            
        } else if (strcmp(type, "double") == 0) {
            temp = double_encode(result, uppercase);
        } else if (strcmp(type, "unicode") == 0) {
            temp = unicode_encode(result, uppercase);
        } else if (strcmp(type, "full") == 0 || strcmp(type, "all") == 0) {
            temp = full_encode(result, uppercase);
            
            if (strcmp(type, "all") == 0) {
                // For "all", do another round of standard encoding
                char* temp2 = url_encode(temp, "standard", 1, uppercase);
                free(temp);
                temp = temp2;
            }
        } else {
            free(result);
            return NULL;
        }
        
        if (!temp) {
            free(result);
            return NULL;
        }
        
        free(result);
        result = temp;
    }
    
    return result;
}

char* double_encode(const char* input, bool uppercase) {
    // First do hex encoding
    char* hex_encoded = full_encode(input, uppercase);
    if (!hex_encoded) return NULL;
    
    // Then encode the result
    char* result = url_encode(hex_encoded, "standard", 1, uppercase);
    free(hex_encoded);
    
    return result;
}

char* unicode_encode(const char* input, bool uppercase) {
    int len = strlen(input);
    int new_len = 0;
    
    // Calculate new length (UTF-8 can be up to 4 bytes per character)
    for (int i = 0; i < len; i++) {
        unsigned char c = input[i];
        if (c < 128) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                new_len++;
            } else {
                new_len += 3;
            }
        } else {
            // UTF-8 multi-byte encoding
            if (c >= 0xC0 && c <= 0xDF) { // 2-byte character
                new_len += 6; // 2 encoded bytes
            } else if (c >= 0xE0 && c <= 0xEF) { // 3-byte character
                new_len += 9; // 3 encoded bytes
            } else if (c >= 0xF0 && c <= 0xF7) { // 4-byte character
                new_len += 12; // 4 encoded bytes
            } else {
                new_len += 3; // Single byte
            }
        }
    }
    
    char* result = malloc(new_len + 1);
    if (!result) return NULL;
    
    int pos = 0;
    for (int i = 0; i < len; i++) {
        unsigned char c = input[i];
        
        // Handle multi-byte UTF-8 sequences
        if ((c & 0x80) != 0) {
            // Copy the UTF-8 bytes as-is with percent encoding
            if (uppercase) {
                pos += snprintf(&result[pos], 4, "%%%02X", c);
            } else {
                pos += snprintf(&result[pos], 4, "%%%02x", c);
            }
            
            // Handle continuation bytes
            if ((c & 0xE0) == 0xC0) { // 2-byte character
                i++;
                if (i < len) {
                    if (uppercase) {
                        pos += snprintf(&result[pos], 4, "%%%02X", input[i]);
                    } else {
                        pos += snprintf(&result[pos], 4, "%%%02x", input[i]);
                    }
                }
            } else if ((c & 0xF0) == 0xE0) { // 3-byte character
                for (int j = 0; j < 2 && i + 1 < len; j++) {
                    i++;
                    if (uppercase) {
                        pos += snprintf(&result[pos], 4, "%%%02X", input[i]);
                    } else {
                        pos += snprintf(&result[pos], 4, "%%%02x", input[i]);
                    }
                }
            } else if ((c & 0xF8) == 0xF0) { // 4-byte character
                for (int j = 0; j < 3 && i + 1 < len; j++) {
                    i++;
                    if (uppercase) {
                        pos += snprintf(&result[pos], 4, "%%%02X", input[i]);
                    } else {
                        pos += snprintf(&result[pos], 4, "%%%02x", input[i]);
                    }
                }
            }
        } else if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            result[pos++] = c;
        } else {
            if (uppercase) {
                pos += snprintf(&result[pos], 4, "%%%02X", c);
            } else {
                pos += snprintf(&result[pos], 4, "%%%02x", c);
            }
        }
    }
    result[pos] = '\0';
    
    return result;
}

char* full_encode(const char* input, bool uppercase) {
    int len = strlen(input);
    char* result = malloc(len * 3 + 1); // Each char can become %XX
    if (!result) return NULL;
    
    int pos = 0;
    for (int i = 0; i < len; i++) {
        if (uppercase) {
            pos += snprintf(&result[pos], 4, "%%%02X", (unsigned char)input[i]);
        } else {
            pos += snprintf(&result[pos], 4, "%%%02x", (unsigned char)input[i]);
        }
    }
    result[pos] = '\0';
    
    return result;
}

char* url_decode(const char* input, int times) {
    if (!input || times < 1) return NULL;
    
    char* result = strdup(input);
    if (!result) return NULL;
    
    for (int t = 0; t < times; t++) {
        int len = strlen(result);
        char* temp = malloc(len + 1); // Decoded string won't be longer
        if (!temp) {
            free(result);
            return NULL;
        }
        
        int pos = 0;
        for (int i = 0; i < len; i++) {
            if (result[i] == '%' && i + 2 < len) {
                char hex1 = result[i + 1];
                char hex2 = result[i + 2];
                
                if (is_hex_digit(hex1) && is_hex_digit(hex2)) {
                    int value = hex_to_int(hex1) * 16 + hex_to_int(hex2);
                    temp[pos++] = (char)value;
                    i += 2;
                } else {
                    temp[pos++] = result[i];
                }
            } else {
                temp[pos++] = result[i];
            }
        }
        temp[pos] = '\0';
        
        free(result);
        result = temp;
    }
    
    return result;
}

char* decode_unicode_escape(const char* input) {
    int len = strlen(input);
    char* result = malloc(len + 1); // Result won't be longer
    if (!result) return NULL;
    
    int pos = 0;
    for (int i = 0; i < len; i++) {
        if (input[i] == '%' && i + 5 < len && 
            tolower(input[i + 1]) == 'u' &&
            is_hex_digit(input[i + 2]) && is_hex_digit(input[i + 3]) &&
            is_hex_digit(input[i + 4]) && is_hex_digit(input[i + 5])) {
            
            int hex_value = (hex_to_int(input[i + 2]) << 12) |
                           (hex_to_int(input[i + 3]) << 8) |
                           (hex_to_int(input[i + 4]) << 4) |
                           hex_to_int(input[i + 5]);
            
            // Convert Unicode code point to UTF-8
            if (hex_value < 0x80) {
                result[pos++] = (char)hex_value;
            } else if (hex_value < 0x800) {
                result[pos++] = (char)(0xC0 | (hex_value >> 6));
                result[pos++] = (char)(0x80 | (hex_value & 0x3F));
            } else if (hex_value < 0x10000) {
                result[pos++] = (char)(0xE0 | (hex_value >> 12));
                result[pos++] = (char)(0x80 | ((hex_value >> 6) & 0x3F));
                result[pos++] = (char)(0x80 | (hex_value & 0x3F));
            }
            
            i += 5;
        } else {
            result[pos++] = input[i];
        }
    }
    result[pos] = '\0';
    
    return result;
}

char* smart_decode(const char* input, bool verbose) {
    if (!input) return NULL;
    
    char* current = strdup(input);
    if (!current) return NULL;
    
    if (verbose && isatty(fileno(stdout))) {
        printf("\nDecoding steps:\n");
    }
    
    for (int pass = 0; pass < MAX_DECODE_PASSES; pass++) {
        bool changed = false;
        
        // Handle %uXXXX decoding
        if (contains_percent_u(current)) {
            char* new_current = decode_unicode_escape(current);
            if (new_current && strcmp(new_current, current) != 0) {
                free(current);
                current = new_current;
                changed = true;
                if (verbose && isatty(fileno(stdout))) {
                    printf("  Pass %d (%%uXXXX decode): %s\n", pass + 1, current);
                }
            } else {
                free(new_current);
            }
        }
        
        // Handle %XX decoding
        char* decoded = url_decode(current, 1);
        if (decoded && strcmp(decoded, current) != 0) {
            free(current);
            current = decoded;
            changed = true;
            if (verbose && isatty(fileno(stdout))) {
                printf("  Pass %d (%%XX decode): %s\n", pass + 1, current);
            }
        } else {
            free(decoded);
        }
        
        if (!changed) {
            break;
        }
    }
    
    return current;
}

bool contains_percent_u(const char* text) {
    if (!text) return false;
    
    int len = strlen(text);
    for (int i = 0; i < len - 5; i++) {
        if (text[i] == '%' && tolower(text[i + 1]) == 'u') {
            // Check if followed by 4 hex digits
            bool all_hex = true;
            for (int j = 0; j < 4; j++) {
                if (!is_hex_digit(text[i + 2 + j])) {
                    all_hex = false;
                    break;
                }
            }
            if (all_hex) return true;
        }
    }
    return false;
}

char* detect_encoding_type(const char* text) {
    if (!text) return "plain";
    
    // Check for any percent signs
    bool has_percent = false;
    for (int i = 0; text[i]; i++) {
        if (text[i] == '%') {
            has_percent = true;
            break;
        }
    }
    
    if (!has_percent) return "plain";
    
    // Check for double encoding (%25XX)
    for (int i = 0; text[i] && text[i + 4]; i++) {
        if (text[i] == '%' && text[i + 1] == '2' && text[i + 2] == '5' &&
            is_hex_digit(text[i + 3]) && is_hex_digit(text[i + 4])) {
            return "double";
        }
    }
    
    // Check for Unicode (percent signs followed by 8-9A-F)
    for (int i = 0; text[i] && text[i + 2]; i++) {
        if (text[i] == '%') {
            char c = toupper(text[i + 1]);
            if (c == '8' || c == '9' || (c >= 'A' && c <= 'F')) {
                return "unicode";
            }
        }
    }
    
    // Check if mostly encoded
    int total_chars = 0;
    int encoded_chars = 0;
    
    for (int i = 0; text[i]; i++) {
        total_chars++;
        if (text[i] == '%' && is_hex_digit(text[i + 1]) && is_hex_digit(text[i + 2])) {
            encoded_chars++;
            i += 2; // Skip the hex digits
        }
    }
    
    if (encoded_chars > total_chars * 0.8) {
        return "full";
    }
    
    return "standard";
}

void analyze_string(const char* input) {
    if (!input) return;
    
    printf("String Analysis:\n");
    printf("  Original: %s\n", input);
    printf("  Length: %zu\n", strlen(input));
    
    // Check for percent signs
    bool has_percent = false;
    int percent_count = 0;
    for (int i = 0; input[i]; i++) {
        if (input[i] == '%') {
            has_percent = true;
            percent_count++;
        }
    }
    
    printf("  Percent Encoded: %s\n", has_percent ? "Yes" : "No");
    
    char* encoding_type = detect_encoding_type(input);
    printf("  Estimated Encoding Type: %s\n", encoding_type);
    
    // Check for Unicode patterns
    bool has_unicode = false;
    for (int i = 0; input[i] && input[i + 2]; i++) {
        if (input[i] == '%') {
            char c = toupper(input[i + 1]);
            if (c == '8' || c == '9' || (c >= 'A' && c <= 'F')) {
                has_unicode = true;
                break;
            }
        }
    }
    printf("  Contains Unicode: %s\n", has_unicode ? "Yes" : "No");
    
    // Check for double encoding
    bool has_double = false;
    for (int i = 0; input[i] && input[i + 4]; i++) {
        if (input[i] == '%' && input[i + 1] == '2' && input[i + 2] == '5' &&
            is_hex_digit(input[i + 3]) && is_hex_digit(input[i + 4])) {
            has_double = true;
            break;
        }
    }
    printf("  Double Encoded: %s\n", has_double ? "Yes" : "No");
    
    if (has_percent) {
        double ratio = (double)percent_count / strlen(input) * 100;
        printf("  Encoded Ratio: %.1f%%\n", ratio);
    }
}

char* to_uppercase_hex(const char* encoded) {
    if (!encoded) return NULL;
    
    int len = strlen(encoded);
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    for (int i = 0; i < len; i++) {
        if (encoded[i] == '%' && i + 2 < len) {
            result[i] = '%';
            result[i + 1] = toupper(encoded[i + 1]);
            result[i + 2] = toupper(encoded[i + 2]);
            i += 2;
        } else {
            result[i] = encoded[i];
        }
    }
    result[len] = '\0';
    
    return result;
}

bool is_stdin_empty(void) {
    // Check if stdin has data without consuming it
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) != 1;
}

char* read_stdin(void) {
    char* buffer = malloc(BUFFER_SIZE);
    if (!buffer) return NULL;
    
    size_t total_size = 0;
    size_t buffer_size = BUFFER_SIZE;
    
    while (1) {
        size_t bytes_read = fread(buffer + total_size, 1, BUFFER_SIZE, stdin);
        if (bytes_read == 0) {
            break;
        }
        
        total_size += bytes_read;
        
        // Resize buffer if needed
        if (total_size + BUFFER_SIZE > buffer_size) {
            buffer_size *= 2;
            char* new_buffer = realloc(buffer, buffer_size);
            if (!new_buffer) {
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
        }
    }
    
    // Null terminate the string
    buffer[total_size] = '\0';
    
    // Remove trailing newline if present
    if (total_size > 0 && buffer[total_size - 1] == '\n') {
        buffer[total_size - 1] = '\0';
        total_size--;
    }
    
    // Trim the buffer to actual size
    char* result = realloc(buffer, total_size + 1);
    if (!result) {
        free(buffer);
        return NULL;
    }
    
    return result;
}

void print_help(void) {
    printf("URL Encoder/Decoder Tool\n");
    printf("========================\n\n");
    printf("Options:\n");
    printf("  -e, --encode TEXT    Text to encode (omit TEXT to read from stdin)\n");
    printf("  -d, --decode TEXT    Text to decode (omit TEXT to read from stdin)\n");
    printf("  -s, --smart TEXT     Smart decode with auto-detection\n");
    printf("  -a, --analyze TEXT   Analyze encoding of text\n");
    printf("  -t, --type TYPE      Encoding type: standard, double, unicode, full, all\n");
    printf("  --times N            Number of times to encode/decode\n");
    printf("  -u, --uppercase      Use uppercase hex letters (default: lowercase)\n");
    printf("  -v, --verbose        Verbose output showing steps\n");
    printf("  -h, --help           Show this help message\n\n");

}