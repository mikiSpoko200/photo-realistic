use nalgebra_glm as glm;

pub struct Ray {
    pub origin: glm::Vec3,
    pub direction: glm::Vec3,
}

impl Ray {
    pub fn new(origin: glm::Vec3, direction: glm::Vec3) -> Self {
        Self {
            origin,
            direction,
        }
    }
}

pub struct IntersectionInfo {
    pub position: glm::Vec3,
}

impl IntersectionInfo {
    pub fn new(position: glm::Vec3) -> Self {
        Self {
            position,
        }
    }
}
