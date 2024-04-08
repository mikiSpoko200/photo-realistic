use std::{marker::PhantomData, rc::Rc};

use crate::{
    aabb::Aabb,
    primitive::Primitive,
};

type Index = usize;

pub trait AxisSelector {
    fn choose<P>(primitives: &[P]) -> usize
    where 
        P: Primitive
    ;
}

#[derive(std::cmp::Ord, std::cmp::PartialOrd, PartialEq, Eq)]
pub enum EdgeType {
    Start,
    End
}

pub struct  Edge {
    t: f32, 
    type_: EdgeType,
}

impl Edge {
    fn new(t: f32, type_: EdgeType) -> Self {
        // check if given split is valid for f32 for comparisons, required by since `Edge` is `Eq`.
        if t != t {
            panic!("split for edge cannot be NaN")
        }
        Self {
            t,
            type_,
        }
    }

    pub fn start(t: f32) -> Self {
        Self::new(t, EdgeType::Start)
    }

    pub fn end(t: f32) -> Self {
        Self::new(t, EdgeType::End)
    }
}

impl std::cmp::PartialEq for Edge {
    fn eq(&self, other: &Self) -> bool {
        // check for NaN
        self.t == other.t && self.type_ == other.type_
    }
}

impl std::cmp::Eq for Edge {}

impl std::cmp::PartialOrd for Edge {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        self.t.partial_cmp(&other.t)
            .map(|ord| ord.then(self.type_.cmp(&other.type_)))
    }
}

impl std::cmp::Ord for Edge {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.partial_cmp(other).expect("edge splits are valid f32 for comparison")
    }
}

pub trait Axis {
    const INDEX: usize;
}

macro_rules! impl_axis {
    ($tt: ty, $index: expr) => {
        impl Axis for $tt {
            const INDEX: usize = $index;
        }
    };
}

macro_rules! pipe {
    ($f: expr) => {
        |x| $f(x)
    };
    ($head: expr => $($tail: expr)=>+) => {
        pipe!(@ |x| $head(x), $($tail)=>+)
    };
    (@ $composed: expr, $head: expr => $($tail: expr)=>+) => {
        pipe!(@ |x| $head($composed(x)), $($tail)=>+)
    };
    (@ $composed: expr, $last: expr) => {
        |x| $last($composed(x))
    }
}

pub struct X;
pub struct Y;
pub struct Z;

impl_axis!(X, 0);
impl_axis!(Y, 1);
impl_axis!(Z, 2);

trait IntoCast: Sized {
    fn cast<A>(self) -> Cast<Self, A> where A: Axis;
}

impl<T> IntoCast for T {
    fn cast<A>(self) -> Cast<Self, A> where A: Axis {
        Cast(self, PhantomData)
    }
}

pub struct Cast<T, A>(T, PhantomData<A>) where A: Axis;

impl<T, A: Axis> Cast<T, A> {
    pub fn component<U>(&self) -> &U where T: AsRef<U> {
        self.0.as_ref()
    }
}

pub type XCast<T> = Cast<T, X>;
pub type YCast<T> = Cast<T, Y>;
pub type ZCast<T> = Cast<T, Z>;

impl<A: Axis> From<Cast<Aabb, A>> for [Edge; 2] {
    fn from(bb: Cast<Aabb, A>) -> Self {
        [Edge::start(bb.0.min[A::INDEX]), Edge::end(bb.0.max[A::INDEX])]
    }
}

pub trait SplitSelector: Sized {
    fn choose(edges: &[Edge]) -> usize;
}

pub struct LargestExtend;

impl AxisSelector for LargestExtend {
    fn choose<P>(primitives: &[P]) -> usize
    where 
        P: Primitive
    {
        (0..crate::N_DIMS)
            .into_iter()
            .map(|axis| {
                let (min, max) = primitives.iter().fold((0.0f32, 0.0f32), |(min, max), primitive| {
                    let bb = primitive.bounding_box();
                    (min.min(bb.min[axis]), max.max(bb.max[axis]))
                });
                (axis, max - min)
            })
            .fold((0, 0.0f32), |(current_best, best_span), (index, span)| if best_span < span { (index, span) } else { (current_best, best_span)})
            .0
    }
}

pub struct SplitMiddle;

impl SplitSelector for SplitMiddle {
    fn choose(edges: &[Edge]) -> usize {
        edges.len() / 2
    }
}

pub struct SAH;

impl SplitSelector for SAH {
    fn choose(edges: &[Edge]) -> usize {
        unimplemented!()
    }
}

#[derive(Clone)]
struct AxisSortedPrimitives<'p, P, const N_DIMS: usize> where P: Primitive {
    pub axis_sorted: [Vec<&'p P>; N_DIMS]
}

pub fn argsort<T: Ord>(data: &[T], cmp: Fn(&T, &T) -> Ordering) -> Vec<usize> {
    let mut indices = (0..data.len()).collect::<Vec<_>>();
    indices.sort_by(cmp).sort_by_key(|&i| &data[i]);
    indices
}

pub fn axis_arg_

impl<'p, P, const N_DIMS: usize> AxisSortedPrimitives<'p, P, N_DIMS>
where
    P: Primitive
{
    pub fn new(primitives: &'p [P]) -> Self {
        Self {
            // TODO: 
            axis_sorted: [
                argsort(data)
            ],
        }
    }

    pub fn split(self, axis: usize, index: usize) -> (AxisSortedPrimitives<'p, P, N_DIMS>, AxisSortedPrimitives<'p, P, N_DIMS>) {
        let mut other = self.clone();
        let (lhs, rhs) = self.axis_sorted[axis].split_at(index);
        self.axis_sorted[axis] = lhs;
        other.views[axis] = rhs;
        (self, other)
    }
}

pub struct Builder<'p, P, A, S>
where
    P: Primitive,
    A: AxisSelector,
    S: SplitSelector,
{
    max_depth: usize,
    bounding_box: Aabb,
    primitives: &'p [P],
    _phantoms: PhantomData<(A, S)>,
}

impl<'p, P, A, S> Builder<'p, P, A, S>
where
    P: Primitive,
    A: AxisSelector,
    S: SplitSelector,
{
    /// Reportedly this is good heuristic to determine max KdTree depth 
    fn depth_heuristic(primitives: &[P]) -> usize {
        8 + (1.3 * (primitives.len() as f32).log2().ceil()) as usize
    }

    fn sort_edges_by_axis<AX: Axis>(primitives: &[P]) -> Vec<Edge> {
        let mut edges = primitives.iter()
            .map(pipe! {
                P::bounding_box => IntoCast::cast::<AX> => <[Edge; 2]>::from
            })
            .flatten()
            .collect::<Vec<_>>();
        edges.sort();
        edges
    }

    fn sort_edges(primitives: &'p [P], axis: usize) -> Vec<Edge> {
        match axis {
            0 => Self::sort_edges_by_axis::<X>(primitives),
            1 => Self::sort_edges_by_axis::<Y>(primitives),
            2 => Self::sort_edges_by_axis::<Z>(primitives),
            _ => panic!("unsuported axis index: {}", axis)
        }
    }

    pub fn new(primitives: &'p [P]) -> Self {
        Self {
            primitives,
            bounding_box: Aabb::from_primitives(primitives),
            _phantoms: PhantomData,
            max_depth: Self::depth_heuristic(primitives),
        }
    }

    fn build_rec(&self, depth: usize, sub_range: &'p [P]) -> Box<KdNode<'p, P>> {
        if depth > self.max_depth {
            return Box::new(KdNode::Leaf(
                match sub_range {
                    [] => KdLeaf::Empty,
                    [p] => KdLeaf::Reference(p),
                    _ => KdLeaf::Slice(sub_range)
                }
            ));
        }

        // choose axis to split
        let axis = A::choose(sub_range);
        let edges = Self::sort_edges(sub_range, axis);
        let split = S::choose(&edges);

        let (lhs, rhs) = sub_range.split();

        Box::new(KdNode::Internal { 
            axis: axis as _, 
            split: split, 
            left: self.build_rec(depth + 1, lhs), 
            right: self.build_rec(depth + 1, rhs)
        })
    }

    fn build(self) -> KdTree<'p, P, A, S> {
        KdTree {
            primitives: self.primitives,
            tree: self.build_rec(0, &self.primitives),
            bounding_box: todo!(),
            _phantoms: PhantomData,
        }
    }
}

pub enum KdLeaf<'p, P> {
    Empty,
    Reference(&'p P),
    Slice(&'p [P])
}

pub enum KdNode<'p, P> {
    Leaf(KdLeaf<'p, P>),
    Internal {
        axis: u8,
        split: f32,
        left: Box<KdNode<'p, P>>,
        right: Box<KdNode<'p, P>>
    }
}

pub struct KdTree<'p, P, A, S> {
    primitives: &'p [P],
    tree: KdNode<'p, P>,
    bounding_box: Aabb,
    _phantoms: PhantomData<(A, S)>
}
