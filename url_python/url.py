#!/usr/bin/env python3
"""
URL Encoding/Decoder Tool
Supports: standard, double, unicode, full encoding, and smart auto-detection
"""

import argparse
import sys
import re
from urllib.parse import quote, unquote
from typing import Tuple, List, Optional

class URLEncoderDecoder:
    def __init__(self):
        self.encoding_types = ['standard', 'double', 'unicode', 'full', 'all']

    def detect_encoding_type(self, text: str) -> str:
        """Auto-detect the encoding type of the input"""
        if not any('%' in text for _ in text):
            return 'plain'
        
        # Check for double encoding (patterns like %25XX)
        double_encoded_pattern = r'%25[0-9A-Fa-f]{2}'
        if re.search(double_encoded_pattern, text):
            return 'double'
        
        # Check for unicode characters
        unicode_pattern = r'%[89ABCDEF][0-9A-Fa-f]'
        if re.search(unicode_pattern, text, re.IGNORECASE):
            return 'unicode'
        
        # Check if all characters are encoded
        encoded_parts = [c for c in text.split('%') if len(c) >= 2]
        if (encoded_parts and 
            all(len(part) >= 2 and part[:2].isalnum() for part in encoded_parts)):
            return 'full'
        
        return 'standard'

    def _hex_encode(self, text: str, uppercase: bool = False) -> str:
        """Helper function to hex encode with case control"""
        if uppercase:
            return ''.join([f'%{ord(c):02X}' for c in text])
        else:
            return ''.join([f'%{ord(c):02x}' for c in text])

    def encode(self, text: str, encoding_type: str = 'standard', 
               times: int = 1, uppercase: bool = False) -> str:
        """Encode text with specified encoding type"""
        if not text:
            return text

        result = text

        for _ in range(times):
            if encoding_type == 'standard':
                result = quote(result, safe='')
                if uppercase:
                    result = result.upper()
            elif encoding_type == 'double':
                # First encode with hex encoding
                if uppercase:
                    result = ''.join([f'%{ord(c):02X}' for c in result])
                else:
                    result = ''.join([f'%{ord(c):02x}' for c in result])
                # Then encode the result again
                result = quote(result, safe='')
                if uppercase:
                    result = result.upper()
            elif encoding_type == 'unicode':
                result = quote(result, safe='', encoding='utf-8')
                if uppercase:
                    result = result.upper()
            elif encoding_type == 'full':
                if uppercase:
                    result = ''.join([f'%{ord(c):02X}' for c in result])
                else:
                    result = ''.join([f'%{ord(c):02x}' for c in result])
            elif encoding_type == 'all':
                if uppercase:
                    result = ''.join([f'%{ord(c):02X}' for c in result])
                else:
                    result = ''.join([f'%{ord(c):02x}' for c in result])
                result = quote(result, safe='')
                if uppercase:
                    result = result.upper()
        
        return result

    def decode(self, text: str, times: int = 1) -> str:
        """Decode URL-encoded text"""
        if not text:
            return text
            
        result = text
        for _ in range(times):
            result = unquote(result)
        return result

    def decode_unicode_escape(self, text: str) -> str:
        """Decode %uXXXX style Unicode escapes"""
        def replace_match(match):
            code = match.group(1)
            try:
                return chr(int(code, 16))
            except Exception:
                return match.group(0)
        return re.sub(r'%u([0-9A-Fa-f]{4})', replace_match, text)

    def smart_decode(self, text: str) -> Tuple[str, List[str]]:
        """Smart decoding with auto-detection and multi-layer support"""
        if not text:
            return text, []
        
        steps = []
        current = text
        max_passes = 20

        for pass_num in range(max_passes):
            changed = False

            # Handle %uXXXX
            if '%u' in current:
                new_current = self.decode_unicode_escape(current)
                if new_current != current:
                    current = new_current
                    steps.append(f"Pass {pass_num + 1} (%uXXXX decode): {current}")
                    changed = True

            # Handle %XX
            new_current = unquote(current)
            if new_current != current:
                current = new_current
                steps.append(f"Pass {pass_num + 1} (%XX decode): {current}")
                changed = True

            if not changed:
                break

        return current, steps

    def analyze_string(self, text: str) -> dict:
        """Analyze a string and provide encoding information"""
        if not text:
            return {'error': 'Empty string provided'}
            
        analysis = {
            'original': text,
            'length': len(text),
            'percent_encoded': '%' in text,
            'estimated_encoding_type': self.detect_encoding_type(text),
            'contains_unicode': bool(re.search(r'%[89ABCDEF][0-9A-Fa-f]', text, re.IGNORECASE)),
            'double_encoded': bool(re.search(r'%25[0-9A-Fa-f]{2}', text))
        }
        
        if analysis['percent_encoded']:
            encoded_chars = text.count('%')
            analysis['encoded_ratio'] = f"{encoded_chars / len(text) * 100:.1f}%"
        
        return analysis


def main():
    # Manual argument parsing for maximum flexibility
    args = argparse.Namespace()
    args.encode = None
    args.decode = None
    args.smart = None
    args.analyze = None
    args.type = 'standard'
    args.times = 1
    args.verbose = False
    args.uppercase = False  # Default is lowercase (False)
    
    # Parse arguments in any order
    i = 1
    while i < len(sys.argv):
        arg = sys.argv[i]
        if arg in ['-e', '--encode']:
            if i + 1 < len(sys.argv) and not sys.argv[i + 1].startswith('-'):
                args.encode = sys.argv[i + 1]
                i += 1
        elif arg in ['-d', '--decode']:
            if i + 1 < len(sys.argv) and not sys.argv[i + 1].startswith('-'):
                args.decode = sys.argv[i + 1]
                i += 1
        elif arg in ['-s', '--smart']:
            if i + 1 < len(sys.argv) and not sys.argv[i + 1].startswith('-'):
                args.smart = sys.argv[i + 1]
                i += 1
        elif arg in ['-a', '--analyze']:
            if i + 1 < len(sys.argv) and not sys.argv[i + 1].startswith('-'):
                args.analyze = sys.argv[i + 1]
                i += 1
        elif arg in ['-t', '--type']:
            if i + 1 < len(sys.argv) and not sys.argv[i + 1].startswith('-'):
                args.type = sys.argv[i + 1]
                i += 1
        elif arg == '--times':
            if i + 1 < len(sys.argv) and sys.argv[i + 1].isdigit():
                args.times = int(sys.argv[i + 1])
                i += 1
        elif arg in ['-u', '--uppercase']:
            args.uppercase = True
        elif arg in ['-v', '--verbose']:
            args.verbose = True
        elif arg in ['-h', '--help']:
            print("URL Encoder/Decoder Tool")
            print("=" * 40)
            print("""
Options:
  -e, --encode TEXT    Text to encode
  -d, --decode TEXT    Text to decode  
  -s, --smart TEXT     Smart decode with auto-detection
  -a, --analyze TEXT   Analyze encoding of text
  -t, --type TYPE      Encoding type: standard, double, unicode, full, all
  --times N            Number of times to encode/decode
  -u, --uppercase      Use uppercase hex letters (default: lowercase)
  -v, --verbose        Verbose output showing steps
  -h, --help           Show this help message
            """)
            
            sys.exit(0)
        i += 1
    
    # Validate arguments
    operations = [args.encode, args.decode, args.smart, args.analyze]
    if not any(operations):
        print("Error: No operation specified. Use -e, -d, -s, or -a")
        print("Use -h for help")
        sys.exit(1)
    
    if len([op for op in operations if op is not None]) > 1:
        print("Error: Only one operation can be specified at a time")
        sys.exit(1)
    
    tool = URLEncoderDecoder()
    
    try:
        if args.encode:
            if not args.encode.strip():
                print("Error: Empty string provided for encoding")
                sys.exit(1)
            result = tool.encode(args.encode, args.type, args.times, args.uppercase)
            case = "UPPERCASE" if args.uppercase else "lowercase"
            print(f"Encoded ({args.type} x{args.times}, {case}): {result}")
            
        elif args.decode:
            if not args.decode.strip():
                print("Error: Empty string provided for decoding")
                sys.exit(1)
            result = tool.decode(args.decode, args.times)
            print(f"Decoded (x{args.times}): {result}")
            
        elif args.smart:
            if not args.smart.strip():
                print("Error: Empty string provided for smart decoding")
                sys.exit(1)
            result, steps = tool.smart_decode(args.smart)
            print(f"Smart decode result: {result}")
            if args.verbose and steps:
                print("\nDecoding steps:")
                for step in steps:
                    print(f"  {step}")
                    
        elif args.analyze:
            if not args.analyze.strip():
                print("Error: Empty string provided for analysis")
                sys.exit(1)
            analysis = tool.analyze_string(args.analyze)
            print("String Analysis:")
            for key, value in analysis.items():
                if key != 'original':
                    print(f"  {key.replace('_', ' ').title()}: {value}")
            print(f"  Original: {analysis['original']}")
                
    except KeyboardInterrupt:
        print("\nOperation cancelled by user")
        sys.exit(130)
    except Exception as e:
        print(f"Error: {str(e)}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()