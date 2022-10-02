#include <engine/hzpch.h>

#include "Camera3D.h"

Camera3D::Camera3D(float fov, float aspect, float near, float far) :
	FOV(fov),
	Near(near),
	Far(far),
	Aspect(aspect)
{
	Projection = glm::scale(glm::perspective(fov, aspect, near, far), { 1, -1, 1 });
	InvProjection = glm::inverse(Projection);
}

void Camera3D::ComputeMatrices()
{
	InvView = glm::inverse(View);
	ProjectionView = Projection * View;
	InvProjectionView = glm::inverse(ProjectionView);
}
