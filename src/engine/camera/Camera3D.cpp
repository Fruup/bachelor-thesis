#include <engine/hzpch.h>

#include "Camera3D.h"

Camera3D::Camera3D(float fov, float aspect, float near, float far) :
	FOV(fov),
	Near(near),
	Far(far),
	Aspect(aspect)
{
	Projection = glm::perspectiveLH(fov, aspect, near, far);
	InvProjection = glm::inverse(Projection);
}

void Camera3D::ComputeMatrices()
{
	InvView = glm::inverse(View);
	ProjectionView = Projection * View;
	InvProjectionView = glm::inverse(ProjectionView);
}
