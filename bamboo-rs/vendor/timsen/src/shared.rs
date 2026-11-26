//! A table or dictionary that is either borrowed or shared, so an input method can keep one per
//! field without tying the field to a lifetime.

use std::ops::Deref;
use std::sync::Arc;

pub(crate) enum Shared<'t, T> {
    Borrowed(&'t T),
    Owned(Arc<T>),
}

impl<T> Deref for Shared<'_, T> {
    type Target = T;
    fn deref(&self) -> &T {
        match self {
            Shared::Borrowed(t) => t,
            Shared::Owned(t) => t,
        }
    }
}

impl<T> Clone for Shared<'_, T> {
    fn clone(&self) -> Self {
        match self {
            Shared::Borrowed(t) => Shared::Borrowed(t),
            Shared::Owned(t) => Shared::Owned(Arc::clone(t)),
        }
    }
}
