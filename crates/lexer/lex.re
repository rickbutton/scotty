#![allow(unused_braces)]
#![allow(redundant_semicolons)]
/*!include:re2c "unicode.re" */

mod lookup;

use crate::mtag::{self};
use crate::token::{*};

macro_rules! emit_token {
    ($lex:ident, $kind:ident) => {
        Some(Token::new(TokenKind::$kind, $lex.cursor - $lex.token_start))
    }
}

#[allow(dead_code)]
fn yydebug(state: usize, c: u8) {
    println!("state: {} char: {} hex: {:02x}", state, char::from(c), c);
}

struct Lexer<'s> {
    s: &'s [u8],

    errors: Vec<Error>,
    token_start: usize,
    cursor: usize,
    mark: usize,
}

impl<'s> Lexer<'s> {
    #[allow(dead_code)]
    pub fn new(s: &'s [u8]) -> Self {
        Lexer { s, errors: vec![], token_start: 0, cursor: 0, mark: 0, }
    }
}

fn hex_escape_value(s: &[u8]) -> u32 {
    let mut value: u32 = 0;
    for b in s.iter() {
        let byte = *b;
        let c = byte as char;
        let v = match c {
            'a'..='f' => byte - 'a' as u8 + 10,
            'A'..='F' => byte - 'A' as u8 + 10,
            '0'..='9' => byte - '0' as u8,
            _ => panic!("unexpected character in hex escape slice"),
        };
        value = (value << 4) | (v & 0xF) as u32;
    }
    value
}

fn check_early_error_id_start_hex_escape(start: usize, end: usize, s: &[u8], errors: &mut Vec<Error>) {
    let value = hex_escape_value(s);
    if !lookup::ID_START.contains(&value) {
        errors.push(Error::new(ErrorKind::EarlyErrorHexEscapeIdStart, start, end));
    }
}
fn check_early_error_id_cont_hex_escape(start: usize, end: usize, s: &[u8], errors: &mut Vec<Error>) {
    let value = hex_escape_value(s);
    if !lookup::ID_CONTINUE.contains(&value) {
        errors.push(Error::new(ErrorKind::EarlyErrorHexEscapeIdContinue, start, end));
    }
}

impl<'s> Iterator for Lexer<'s> {
    type Item = Token;

    fn next(&mut self) -> Option<Self::Item> {
        let mut mt: mtag::MtagTrie = Vec::new();

        /*!stags:re2c format = 'let mut @@{tag} = NONE;'; */
        const NONE: usize = std::usize::MAX;
        let (id_hash, mut idstart_hex0, idstart_hex1);
        
        /*!mtags:re2c format = 'let mut @@ = mtag::MTAG_ROOT;'; */
        let (idcont_hex0, idcont_hex1);
        self.token_start = self.cursor;

        /*!re2c
            re2c:define:YYCTYPE      = u8;
            re2c:define:YYPEEK       = "if self.cursor<self.s.len(){*self.s.get_unchecked(self.cursor)}else{0}";
            re2c:define:YYSKIP       = "self.cursor += 1;";
            re2c:define:YYBACKUP     = "self.mark = self.cursor;";
            re2c:define:YYRESTORE    = "self.cursor = self.mark;";
            re2c:define:YYLESSTHAN   = "self.cursor >= self.s.len()";
            re2c:define:YYSTAGP      = "@@{tag} = self.cursor;";
            re2c:define:YYSTAGN      = "@@{tag} = NONE;";
            re2c:define:YYSHIFTSTAG  = "@@{tag} -= -@@{shift}isize as usize;";
            re2c:define:YYMTAGP      = "@@ = mtag::add_mtag(&mut mt, @@, self.cursor);";
            re2c:define:YYMTAGN      = "@@ = mtag::add_mtag(&mut mt, @@, mtag::NONE);";
            re2c:eof                 = 0;
            re2c:yyfill:enable       = 0;

            re2c:indent:string       = "    ";
            re2c:indent:top          = 2;

            // helper conditions
            DecimalDigit = [0-9];
            HexDigit = [0-9a-fA-F];
            Hex4Digits = HexDigit{4};
            OctalDigit = [0-7];
            NonOctalDigit = [89];
            
            /* 12.2 White Space https://tc39.es/ecma262/#sec-white-space */
            /* 12.3 Line Terminators https://tc39.es/ecma262/#sec-line-terminators */
            /* 12.4 Comments https://tc39.es/ecma262/#sec-comments */
            WhiteSpace = (Zs | [\t\u000B\u000C\uFEFF])+;
            WhiteSpace { return emit_token!(self, WhiteSpace); }

            LineTerminator = [\n] | "\r\n" | [\r\u2028\u2029];
            LineTerminator { return emit_token!(self, LineTerminator); }

            MultiLineComment = "/" "*" ([^\x00*]|"*"[^\x00/])* "*"+ "/";
            MultiLineComment { return emit_token!(self, MultiLineComment); }

            SingleLineComment = "//" [^\u0000\n\r\u2028\u2029]*;
            SingleLineComment { return emit_token!(self, SingleLineComment); }

            // 12.8.1 Null Literals
            NullLiteral = "null";
            NullLiteral { return emit_token!(self, NullLiteral); }

            // 12.8.2 Boolean Literals
            BooleanLiteral = ("true"|"false");
            BooleanLiteral { return emit_token!(self, BooleanLiteral); }

            // TODO: u { CodePoint }
            UnicodeEscapeSequence_IdentifierStart = "\\u" @idstart_hex0 Hex4Digits @idstart_hex1;
            UnicodeEscapeSequence_IdentifierPart = "\\u" #idcont_hex0 Hex4Digits #idcont_hex1;

            /* 12.6 Names and Keywords https://tc39.es/ecma262/#sec-names-and-keywords */
            IdentifierStartChar = ID_Start | [$_];
            IdentifierPartChar = ID_Continue | [$\u200c\u200d];
            IdentifierStart = UnicodeEscapeSequence_IdentifierStart | IdentifierStartChar;
            IdentifierPart = UnicodeEscapeSequence_IdentifierPart | IdentifierPartChar;
            IdentifierName = (@id_hash "#")? IdentifierStart (IdentifierPart*);
            
            IdentifierName { 
                /* 12.6.1.1 Static Semantics: Early Errors
                 * https://tc39.es/ecma262/#sec-identifier-names-static-semantics-early-errors
                 */
                let cont_hex_escapes = mtag::SliceIter::new(&mt, idcont_hex0, idcont_hex1);

                assert!((idstart_hex0 == NONE) == (idstart_hex1 == NONE));
                if idstart_hex0 != NONE && idstart_hex1 != NONE {
                    let escape = &self.s[idstart_hex0..idstart_hex1];
                    check_early_error_id_start_hex_escape(idstart_hex0, idstart_hex1, escape, &mut self.errors);
                }
                for tag_range in cont_hex_escapes {
                    if let Some((start, end)) = tag_range {
                        let escape = &self.s[start..end];
                        check_early_error_id_cont_hex_escape(start, end, escape, &mut self.errors);
                    }
                }

                if id_hash != NONE {
                    return emit_token!(self, PrivateIdentifier);
                } else {
                    return emit_token!(self, IdentifierName);
                }
            }

            /* 12.7 Punctuators https://tc39.es/ecma262/#sec-punctuators */

            // 12.8.3 Numeric Literals TODO

            // 12.8.4 String Literals TODO
            SingleEscapeCharacter = ['"\\bfnrtv];
            EscapeCharacter = SingleEscapeCharacter | DecimalDigit | [xu];
            NonEscapeCharacter = . \ EscapeCharacter;
            CharacterEscapeSequence = SingleEscapeCharacter | NonEscapeCharacter;
            EscapeSequence_Null = "0";
            LegacyOctalEscapeSequence = "XXX"; // TODO
            NonOctalDecimalEscapeSequence = "XXX"; // TODO
            HexEscapeSequence = "x" HexDigit HexDigit;
            // TODO: try to share tag code path with identifiers?
            UnicodeEscapeSequence_DoubleStringCharacter = "XXX"; // TODO
            UnicodeEscapeSequence_SingleStringCharacter = "XXX"; // TODO
            EscapeSequence_DoubleStringCharacter = CharacterEscapeSequence |
                                                   EscapeSequence_Null |
                                                   LegacyOctalEscapeSequence |
                                                   NonOctalDecimalEscapeSequence |
                                                   HexEscapeSequence |
                                                   UnicodeEscapeSequence_DoubleStringCharacter;
            EscapeSequence_SingleStringCharacter = CharacterEscapeSequence |
                                                   EscapeSequence_Null |
                                                   LegacyOctalEscapeSequence |
                                                   NonOctalDecimalEscapeSequence |
                                                   HexEscapeSequence |
                                                   UnicodeEscapeSequence_SingleStringCharacter;
            LineContinuation = "\\" LineTerminator;
            DoubleStringCharacter = ("\\" EscapeSequence_DoubleStringCharacter) |
                                    LineContinuation |
                                    [^"\\\n\r];
            SingleStringCharacter = ("\\" EscapeSequence_SingleStringCharacter) |
                                    LineContinuation |
                                    [^'\\\n\r];
            StringLiteral = ("\"" (DoubleStringCharacter*) "\"") |
                            ("'"  (SingleStringCharacter*) "'");

            // - 12.8.4.1 Static Semantics: Early Errors TODO
            StringLiteral {
                return emit_token!(self, StringLiteral);
            }

            // 12.8.5 Regular Expression Literals
            RegularExpressionNonTerminator = [^\n\r\u2028\u2029];
            RegularExpressionBackslashSequence = "\\" RegularExpressionNonTerminator;
            RegularExpressionClassChar = (RegularExpressionNonTerminator \ [\\\]]) |
                                         RegularExpressionBackslashSequence;
            RegularExpressionClass = "[" (RegularExpressionClassChar*) "]";
            RegularExpressionFirstChar = (RegularExpressionNonTerminator \ [*\\/[]) |
                                         RegularExpressionBackslashSequence |
                                         RegularExpressionClass;
            RegularExpressionChar = (RegularExpressionNonTerminator \ [\\/[]) |
                                    RegularExpressionBackslashSequence |
                                    RegularExpressionClass;
            RegularExpressionFlags = IdentifierPartChar*;
            RegularExpressionBody = RegularExpressionFirstChar (RegularExpressionChar*);
            RegularExpressionLiteral = "/" RegularExpressionBody "/" RegularExpressionFlags;
            RegularExpressionLiteral {
                return emit_token!(self, RegularExpressionLiteral);
            }


            // 12.8.6 Template Literals TODO
            // 12.8.7 Punctuation TODO

            * { 
                let start = self.cursor;
                let end = start + 1;
                self.errors.push(Error::new(ErrorKind::UnexpectedInput, start, end));
                return emit_token!(self, Error);
            }
            $ { return None; }
         */
    }
}

/* #[test] */
/* fn whitespace() { */
/*     let str = "/1* foo \n *1/\n//foo \\ bar\n b"; */
/*     let lexer = Lexer::new(str.as_bytes()); */
/*     assert_eq!(lexer.collect::<Vec<_>>(), vec![ */
/*         token!(MultiLineComment).expect("?"), */
/*         token!(LineTerminator).expect("?"), */
/*         token!(SingleLineComment).expect("?"), */
/*         token!(LineTerminator).expect("?"), */
/*         token!(WhiteSpace).expect("?"), */
/*         token!(Error).expect("?"), */
/*     ]); */
/* } */

#[test]
fn idents() {
    let str = "\\u0061bar \\u0031baz b\\u0032ooao b\\u005eb";
    let mut lexer = Lexer::new(str.as_bytes());
    assert_eq!(lexer.by_ref().collect::<Vec<_>>(), vec![
        token!(IdentifierName, 9),
        token!(WhiteSpace, 1),
        token!(IdentifierName, 9),
        token!(WhiteSpace, 1),
        token!(IdentifierName, 11),
        token!(WhiteSpace, 1),
        token!(IdentifierName, 8),
    ]);
    assert_eq!(&lexer.errors, &vec![
        error!(EarlyErrorHexEscapeIdStart, 12, 16),
        error!(EarlyErrorHexEscapeIdContinue, 35, 39),
    ]);
}

#[test]
fn private_ident() {
    let str = "#hello";
    let mut lexer = Lexer::new(str.as_bytes());
    assert_eq!(lexer.by_ref().collect::<Vec<_>>(), vec![ token!(PrivateIdentifier, 6), ]);
    assert_eq!(&lexer.errors, &vec![]);
}

#[test]
fn string() {
    let str = "'hello world'";
    let mut lexer = Lexer::new(str.as_bytes());
    assert_eq!(lexer.by_ref().collect::<Vec<_>>(), vec![ token!(StringLiteral, 13), ]);
    assert_eq!(&lexer.errors, &vec![]);
}

#[test]
fn regex() {
    let str = "/hello world/ufo";
    let mut lexer = Lexer::new(str.as_bytes());
    assert_eq!(lexer.by_ref().collect::<Vec<_>>(), vec![ token!(RegularExpressionLiteral, 16), ]);
    assert_eq!(&lexer.errors, &vec![]);
}
