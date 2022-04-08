#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum ErrorKind {
    UnexpectedInput,
    EarlyErrorHexEscapeIdStart,
    EarlyErrorHexEscapeIdContinue,
}
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct Error {
    kind: ErrorKind,
    start: usize,
    end: usize,
}

impl Error {
    pub fn new(kind: ErrorKind, start: usize, end: usize) -> Self {
        Error { kind, start, end }
    }
}
macro_rules! error {
    ($kind:ident, $start:expr, $end:expr) => {
        Error::new(ErrorKind::$kind, $start, $end)
    };
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum TokenKind {
    Error,

    WhiteSpace,
    LineTerminator,
    MultiLineComment,
    SingleLineComment,

    NullLiteral,
    BooleanLiteral,
    StringLiteral,

    IdentifierName,
    PrivateIdentifier,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct Token {
    pub kind: TokenKind,
    pub len: usize,
}

impl Token {
    pub(crate) fn one(kind: TokenKind) -> Token {
        Token { kind, len: 1 }
    }
    pub(crate) fn new(kind: TokenKind, len: usize) -> Token {
        Token { kind, len }
    }
}

macro_rules! token {
    ($kind:ident, $len:literal) => {
        Token::new(TokenKind::$kind, $len)
    };
    ($kind:ident, $len:literal) => {
        Token::new(TokenKind::$kind, $len)
    };
}
pub(crate) use error;
pub(crate) use token;
