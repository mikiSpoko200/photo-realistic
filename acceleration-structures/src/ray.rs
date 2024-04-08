use crate::Float;
use nalgebra::Vector3;

pub struct Ray {
    pub origin: Vector3<Float>,
    pub direction: Vector3<Float>,
    pub t_max: Float,
}
