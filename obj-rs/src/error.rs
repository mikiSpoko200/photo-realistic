//! Contains helper structs for error handling

use std::convert::From;
use std::error::Error;
use std::fmt;
use std::io;
use std::num::{ParseFloatError, ParseIntError};

/// A type for results generated by `load_obj` and `load_mtl` where the `Err` type is hard-wired to
/// `ObjError`
///
/// This typedef is generally used to avoid writing out `ObjError` directly and is otherwise a
/// direct mapping to `std::result::Result`.
pub type ObjResult<T> = Result<T, ObjError>;

/// The error type for loading of the `obj` file.
#[derive(Debug)]
pub enum ObjError {
    /// IO error has been occurred during opening the `obj` file.
    Io(io::Error),
    /// Tried to parse integer from the `obj` file, but failed.
    ParseInt(ParseIntError),
    /// Tried to parse floating point number from the `obj` file, but failed.
    ParseFloat(ParseFloatError),
    /// `LoadError` has been occurred during parsing the `obj` file.
    Load(LoadError),
}

macro_rules! implement {
    ($name:ident, $error:path) => {
        impl From<$error> for ObjError {
            fn from(err: $error) -> Self {
                ObjError::$name(err)
            }
        }
    };
}

impl fmt::Display for ObjError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ObjError::Io(ref e) => e.fmt(f),
            ObjError::ParseInt(ref e) => e.fmt(f),
            ObjError::ParseFloat(ref e) => e.fmt(f),
            ObjError::Load(ref e) => e.fmt(f),
        }
    }
}

impl Error for ObjError {
    fn cause(&self) -> Option<&dyn Error> {
        match *self {
            ObjError::Io(ref err) => Some(err),
            ObjError::ParseInt(ref err) => Some(err),
            ObjError::ParseFloat(ref err) => Some(err),
            ObjError::Load(ref err) => Some(err),
        }
    }
}

implement!(Io, io::Error);
implement!(ParseInt, ParseIntError);
implement!(ParseFloat, ParseFloatError);
implement!(Load, LoadError);

/// The error type for parse operations of the `Obj` struct.
#[derive(PartialEq, Eq, Clone, Debug)]
pub struct LoadError {
    kind: LoadErrorKind,
    message: &'static str,
}

impl LoadError {
    /// Outputs the detailed cause of loading an OBJ failing.
    pub fn kind(&self) -> &LoadErrorKind {
        &self.kind
    }
}

/// Enum to store the various types of errors that can cause loading an OBJ to fail.
#[derive(Copy, PartialEq, Eq, Clone, Debug)]
pub enum LoadErrorKind {
    /// Met unexpected statement.
    UnexpectedStatement,
    /// Received wrong number of arguments.
    WrongNumberOfArguments,
    /// Received unexpected type of arguments.
    WrongTypeOfArguments,
    /// Model should be triangulated first to be loaded properly.
    UntriangulatedModel,
    /// Model cannot be transformed into requested form.
    InsufficientData,
    /// Received index value out of range.
    IndexOutOfRange,
    /// A line is expected after the backslash (\).
    BackslashAtEOF,
    /// Group number exceeded limitation.
    TooBigGroupNumber,
}

impl LoadError {
    /// Creates a new custom error from a specified kind and message.
    pub fn new(kind: LoadErrorKind, message: &'static str) -> Self {
        LoadError { kind, message }
    }
}

impl Error for LoadError {}

impl fmt::Display for LoadError {
    fn fmt(&self, fmt: &mut fmt::Formatter<'_>) -> fmt::Result {
        use LoadErrorKind::*;

        let msg = match self.kind {
            UnexpectedStatement => "Met unexpected statement",
            WrongNumberOfArguments => "Received wrong number of arguments",
            WrongTypeOfArguments => "Received unexpected type of arguments",
            UntriangulatedModel => "Model should be triangulated first to be loaded properly",
            InsufficientData => "Model cannot be transformed into requested form",
            IndexOutOfRange => "Received index value out of range",
            BackslashAtEOF => r"A line is expected after the backslash (\)",
            TooBigGroupNumber => "Group number exceeded limitation.",
        };

        write!(fmt, "{}: {}", msg, self.message)
    }
}

macro_rules! make_error {
    ($kind:ident, $message:expr) => {
        return Err(::std::convert::From::from($crate::error::LoadError::new(
            $crate::error::LoadErrorKind::$kind,
            $message,
        )))
    };
}

pub(crate) use make_error;
