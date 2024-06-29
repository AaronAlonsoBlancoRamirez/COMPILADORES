from enum import Enum
from collections import namedtuple
import re

class TokenType(Enum):
    NEWLINE = 1
    HEADER = 2
    BOLD = 3
    ITALIC = 4
    CODE_INLINE = 5
    CITATION = 6
    LIST = 7
    COMMENT = 8
    LINK = 9
    IMAGE = 10
    TEXT = 11
    MARKDOWN_CODE_BLOCK = 12
    ERROR = 13
    EOF = 14

Token = namedtuple('Token', ['type', 'value', 'line', 'column'])

def tokenize(text):
    tokens = []
    lines = text.split('\n')
    line_number = 0
    token_patterns = [
        (r'^#{1,3} (.*)', TokenType.HEADER),
        (r'\*\*(.*?)\*\*', TokenType.BOLD),
        (r'\*(.*?)\*', TokenType.ITALIC),
        (r'`(.*?)`', TokenType.CODE_INLINE),
        (r'^> (.*)', TokenType.CITATION),
        (r'^- (.*)', TokenType.LIST),
        (r'\[([^\]]+)\]\(([^\)]+)\)', TokenType.LINK),
        (r'!\[([^\]]*)\]\(([^\)]+)\)', TokenType.IMAGE),
        (r'^(.+)$', TokenType.TEXT),
    ]
    for line in lines:
        line_number += 1
        start = 0
        while start < len(line):
            for pattern, type_ in token_patterns:
                match = re.match(pattern, line[start:])
                if match:
                    tokens.append(Token(type_, match.groups() if type_ in {TokenType.LINK, TokenType.IMAGE} else match.group(1), line_number, match.start()))
                    start += match.end()
                    break
            else:
                if line[start:].strip():
                    tokens.append(Token(TokenType.TEXT, line[start:], line_number, start))
                break
        tokens.append(Token(TokenType.NEWLINE, '', line_number, len(line)))
    tokens.append(Token(TokenType.EOF, '', line_number + 1, 0))
    
    # Print all generated tokens
    print("Generated tokens:")
    for token in tokens:
        print(token)
    
    return tokens

class ParserLL1:
    def __init__(self, tokens):
        self.tokens = tokens
        self.current_token_index = 0
        self.current_token = tokens[0] if tokens else Token(TokenType.EOF, '', 0, 0)
        self.errors = []
        self.stack = ['EOF', 'Documento']
        self.parse_table = {
            ('Documento', 'HEADER'): ['Parrafo', 'EOF'],
            ('Documento', 'EOF'): ['EOF'],
            
            ('Parrafo', 'HEADER'): ['Encabezado', 'ParrafoOpcional'],
            ('Parrafo', 'TEXT'): ['Texto', 'ParrafoOpcional'],
            ('Parrafo', 'LIST'): ['Lista', 'ParrafoOpcional'],
            ('Parrafo', 'CITATION'): ['Cita', 'ParrafoOpcional'],
            ('Parrafo', 'EOF'): [],
            ('Parrafo', 'NEWLINE'): ['NEWLINE'],

            ('ParrafoOpcional', 'HEADER'): ['Parrafo'],
            ('ParrafoOpcional', 'TEXT'): ['Parrafo'],
            ('ParrafoOpcional', 'LIST'): ['Parrafo'],
            ('ParrafoOpcional', 'CITATION'): ['Parrafo'],
            ('ParrafoOpcional', 'NEWLINE'): [],
            ('ParrafoOpcional', 'EOF'): [],

            ('Encabezado', 'HEADER'): ['Texto', 'NEWLINE', 'ParrafoOpcional'],
            ('Encabezado', 'TEXT'): ['Texto', 'NEWLINE', 'ParrafoOpcional'],
            ('Encabezado', 'CITATION'): ['Cita', 'ParrafoOpcional'],
            ('Encabezado', 'LIST'): ['Lista', 'NEWLINE', 'ParrafoOpcional'],
            ('Encabezado', 'EOF'): [],

            ('Texto', 'TEXT'): ['Texto', 'NEWLINE'],
            ('Texto', 'LIST'): ['Lista', 'NEWLINE'],
            ('Texto', 'HEADER'): ['Parrafo'],
            ('Texto', 'NEWLINE'): [],
            ('Texto', 'EOF'): [],

            ('Lista', 'LIST'): ['Lista', 'NEWLINE'],
            ('Lista', 'TEXT'): ['Texto', 'NEWLINE'],
            ('Lista', 'HEADER'): ['Parrafo'],
            ('Lista', 'CITATION'): ['Cita', 'ParrafoOpcional'],
            ('Lista', 'NEWLINE'): [],
            ('Lista', 'EOF'): [],

            ('Cita', 'CITATION'): ['Cita', 'NEWLINE'],
            ('Cita', 'TEXT'): ['Texto', 'NEWLINE'],
            ('Cita', 'HEADER'): ['Parrafo'],
            ('Cita', 'NEWLINE'): [],
            ('Cita', 'EOF'): [],

            ('NEWLINE', 'HEADER'): ['Parrafo'],
            ('NEWLINE', 'TEXT'): ['Texto'],
            ('NEWLINE', 'LIST'): ['Lista'],
            ('NEWLINE', 'CITATION'): ['Cita'],
            ('NEWLINE', 'NEWLINE'): ['NEWLINE'],
            ('NEWLINE', 'EOF'): [],
        }
        self.sync_tokens = {TokenType.EOF, TokenType.NEWLINE, TokenType.HEADER, TokenType.TEXT, TokenType.LIST, TokenType.CITATION}

    def parse(self):
        print(f"\nStarting parsing with token: {self.current_token}\n")
        while self.stack:
            top_of_stack = self.stack[-1]
            print(f"Top of stack: {top_of_stack}")
            print(f"Current Token: {self.current_token.type} ({self.current_token.value})")
            if top_of_stack == 'EOF' and self.current_token.type == TokenType.EOF:
                print("Found EOF token and top of stack is EOF")
                self.stack.pop()
                if not self.stack:
                    print("\nParsing completed successfully.\n")
                    return
                else:
                    self.report_error("Stack not empty after parsing complete")
                    return
            action = self.parse_table.get((top_of_stack, self.current_token.type.name))
            print(f"Action: {action}\n")
            if action is None:
                self.report_error(f"Parsing error: No action for top of stack {top_of_stack} and current token {self.current_token.type}")
                self.panic_mode_recovery()
                continue
            self.stack.pop()
            if action:
                for rule in reversed(action):
                    if rule and rule != 'EOF':  
                        self.stack.append(rule)
            print(f"Stack after action: {self.stack}\n")
            if not action or (action == [] and self.current_token.type == TokenType.EOF):
                self.advance()
            elif action and top_of_stack != 'ParrafoOpcional' and top_of_stack != 'NEWLINE':
                self.advance()
        if len(self.stack) == 1 and self.stack[0] == 'EOF':
            print("\nParsing completed successfully.\n")
            return
        self.report_error("Parsing completed but stack is not empty")

    def advance(self):
        if self.current_token_index < len(self.tokens) - 1:
            self.current_token_index += 1
            self.current_token = self.tokens[self.current_token_index]
        else:
            self.current_token = Token(TokenType.EOF, '', self.current_token.line + 1, 0)

    def report_error(self, message):
        self.errors.append(message)

    def panic_mode_recovery(self):
        while self.current_token.type not in self.sync_tokens and self.current_token.type != TokenType.EOF:
            self.advance()
        if self.current_token.type == TokenType.EOF:
            return
        self.advance()

    def translate_to_html(self):
        html_output = []
        list_open = False
        for token in self.tokens:
            if token.type == TokenType.HEADER:
                level = token.value.count('#')
                html_output.append(f'<h{level}>{token.value.strip("# ").strip()}</h{level}>')
            elif token.type == TokenType.BOLD:
                html_output.append(f'<strong>{token.value}</strong>')
            elif token.type == TokenType.ITALIC:
                html_output.append(f'<em>{token.value}</em>')
            elif token.type == TokenType.CODE_INLINE:
                html_output.append(f'<code>{token.value}</code>')
            elif token.type == TokenType.CITATION:
                html_output.append(f'<blockquote>{token.value}</blockquote>')
            elif token.type == TokenType.LIST:
                if not list_open:
                    html_output.append('<ul>')
                    list_open = True
                html_output.append(f'<li>{token.value}</li>')
            elif token.type == TokenType.LINK:
                link_text, link_url = token.value
                html_output.append(f'<a href="{link_url}">{link_text}</a>')
            elif token.type == TokenType.IMAGE:
                alt_text, img_url = token.value
                html_output.append(f'<img src="{img_url}" alt="{alt_text}">')
            elif token.type == TokenType.TEXT:
                html_output.append(f'<p>{token.value}</p>')
            elif token.type == TokenType.NEWLINE:
                if list_open:
                    html_output.append('</ul>')
                    list_open = False
            elif token.type == TokenType.EOF:
                break
        if list_open:
            html_output.append('</ul>')
        return '\n'.join(html_output)

    def translate_to_latex(self):
        latex_output = []
        list_open = False
        for token in self.tokens:
            if token.type == TokenType.HEADER:
                level = token.value.count('#')
                section_type = 'section' if level == 1 else 'subsection' if level == 2 else 'subsubsection'
                latex_output.append(f'\\{section_type}{{{token.value.strip("# ").strip()}}}')
            elif token.type == TokenType.BOLD:
                latex_output.append(f'\\textbf{{{token.value}}}')
            elif token.type == TokenType.ITALIC:
                latex_output.append(f'\\textit{{{token.value}}}')
            elif token.type == TokenType.CODE_INLINE:
                latex_output.append(f'\\texttt{{{token.value}}}')
            elif token.type == TokenType.CITATION:
                latex_output.append(f'\\begin{{quote}}{token.value}\\end{{quote}}')
            elif token.type == TokenType.LIST:
                if not list_open:
                    latex_output.append('\\begin{itemize}')
                    list_open = True
                latex_output.append(f'\\item {token.value}')
            elif token.type == TokenType.LINK:
                link_text, link_url = token.value
                latex_output.append(f'\\href{{{link_url}}}{{{link_text}}}')
            elif token.type == TokenType.IMAGE:
                alt_text, img_url = token.value
                latex_output.append(f'\\begin{{figure}}\\includegraphics[width=\\linewidth]{{{img_url}}}\\caption{{{alt_text}}}\\end{{figure}}')
            elif token.type == TokenType.TEXT:
                latex_output.append(token.value)
            elif token.type == TokenType.NEWLINE:
                if list_open:
                    latex_output.append('\\end{itemize}')
                    list_open = False
            elif token.type == TokenType.EOF:
                break
        if list_open:
            latex_output.append('\\end{itemize}')
        return '\n'.join(latex_output)

def main():
    with open("test3.txt", "r", encoding="utf-8") as file:
        text = file.read()

    tokens = tokenize(text)
    
    parser = ParserLL1(tokens)
    parser.parse()
    if parser.errors:
        print("Errors encountered during parsing:")
        for error in parser.errors:
            print(error)
    
    html_output = parser.translate_to_html()
    latex_output = parser.translate_to_latex()
    
    print("\nHTML Output:\n")
    print(html_output)
    print("\nLaTeX Output:\n")
    print(latex_output)

if __name__ == "__main__":
    main()
