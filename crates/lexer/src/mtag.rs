pub(crate) const NONE: usize = std::usize::MAX;
pub(crate) const MTAG_ROOT: usize = NONE - 1;

pub(crate) type MtagTrie = Vec<MtagElem>;
pub(crate) struct MtagElem {
    elem: usize, // tag value
    pred: usize, // index of the predecessor node or root
}
pub(crate) fn add_mtag(trie: &mut MtagTrie, mtag: usize, value: usize) -> usize {
    trie.push(MtagElem {
        elem: value,
        pred: mtag,
    });
    return trie.len() - 1;
}

pub(crate) struct SingleIter<'a> {
    trie: &'a MtagTrie,
    x: usize,
}
impl<'a> SingleIter<'a> {
    #[allow(dead_code)]
    pub(crate) fn new(trie: &'a MtagTrie, x: usize) -> Self {
        SingleIter { trie, x }
    }
}
impl<'a> Iterator for SingleIter<'a> {
    type Item = usize;

    fn next(&mut self) -> Option<Self::Item> {
        if self.x == MTAG_ROOT {
            return None;
        }

        let x = self.x;
        let ex = self.trie[x].elem;
        self.x = self.trie[x].pred;

        Some(ex)
    }
}

pub(crate) struct SliceIter<'a> {
    trie: &'a MtagTrie,
    x: usize,
    y: usize,
}
impl<'a> SliceIter<'a> {
    pub(crate) fn new(trie: &'a MtagTrie, x: usize, y: usize) -> Self {
        SliceIter { trie, x, y }
    }
}
impl<'a> Iterator for SliceIter<'a> {
    type Item = Option<(usize, usize)>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.x == MTAG_ROOT && self.y == MTAG_ROOT {
            return None;
        }
        assert!(self.x != MTAG_ROOT && self.y != MTAG_ROOT);

        let x = self.x;
        let y = self.y;
        let ex = self.trie[x].elem;
        let ey = self.trie[y].elem;

        self.x = self.trie[x].pred;
        self.y = self.trie[y].pred;

        assert!((ex == NONE) == (ey == NONE));
        if ex != NONE && ey != NONE {
            Some(Some((ex, ey)))
        } else {
            Some(None)
        }
    }
}
