use serde_derive::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, PartialEq, Eq, Debug, Clone, Copy, Hash)]
#[repr(C)] 
pub struct TagSeg {
    pub sign: bool,
    pub begin: u32,
    pub end: u32,
}