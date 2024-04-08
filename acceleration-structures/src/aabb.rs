use std::ops::RangeInclusive;

pub type AxisBounds = RangeInclusive<f32>;

/// N dimentional Axis Aligned Bounding Box
pub struct Aabb<const N_DIMS: usize> {
    axis_bounds: [AxisBounds; N_DIMS]
}

impl<const N_DIMS: usize> From<[AxisBounds; N_DIMS]> for Aabb<N_DIMS> {
    fn from(axis_bounds: [AxisBounds; N_DIMS]) -> Self {
        Self { axis_bounds }
    }
}

impl Aabb<1> {
    pub fn x(&self) -> &AxisBounds {
        &self.axis_bounds[0]
    }
}

impl Aabb<2> {
    pub fn x(&self) -> &AxisBounds {
        &self.axis_bounds[0]
    }

    pub fn y(&self) -> &AxisBounds {
        &self.axis_bounds[1]
    }
}

impl Aabb<3> {
    pub fn x(&self) -> &AxisBounds {
        &self.axis_bounds[0]
    }

    pub fn y(&self) -> &AxisBounds {
        &self.axis_bounds[1]
    }
    
    pub fn z(&self) -> &AxisBounds {
        &self.axis_bounds[2]
    }

    pub fn new(xs: RangeInclusive<f32>, ys: RangeInclusive<f32>, zs: RangeInclusive<f32>) -> Self {
        Self {
            axis_bounds: [xs, ys, zs],
        }
    }
}

pub type Aabb1 = Aabb<1>;
pub type Aabb2 = Aabb<2>;
pub type Aabb3 = Aabb<3>;
