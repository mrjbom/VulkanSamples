#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#pragma warning(push, 0)
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)

class Camera
{
private:
    float fov;
    float znear, zfar;

    void updateViewMatrix()
    {
        if (type == CameraType::firstperson) {
            cameraViewAt = position + cameraFront;
        }
        else {
            cameraViewAt = glm::vec3(0.0f);
            position.x = -lookAtDistance * (cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y)));
            position.y = -lookAtDistance * sin(glm::radians(rotation.x));
            position.z = -lookAtDistance * (cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y)));
            // TODO
            // Based on the "position", move the point the camera is looking at and the camera position itself,
            // so that we can rotate not only around the center(0, 0, 0)
        }

        matrices.view = glm::lookAt(
            position,
            cameraViewAt,
            worldUp
        );
    }
private:
    float lookAtDistance = 0.0f;
    glm::vec3 cameraFront = glm::vec3(0.0f);
    glm::vec3 cameraViewAt = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
public:
    enum CameraType { lookat, firstperson };
    CameraType type = CameraType::lookat;

    float rotationSpeed = 1.0f;
    float movementSpeed = 1.0f;

    struct
    {
        glm::mat4 perspective = glm::mat4(1.0f);
        glm::mat4 view = glm::mat4(1.0f);
    } matrices;

    struct
    {
        bool left = false;
        bool right = false;
        bool up = false;
        bool down = false;
        bool shift = false;
    } keys;

    float getNearClip() {
        return znear;
    }

    float getFarClip() {
        return zfar;
    }

    void setPerspective(float fov, float aspect, float znear, float zfar)
    {
        this->fov = fov;
        this->znear = znear;
        this->zfar = zfar;
        matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
    };

    void updateAspectRatio(float aspect)
    {
        matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
    }

    void setPosition(glm::vec3 position)
    {
        this->position = position;
        updateViewMatrix();
    }

    glm::vec3 getPosition()
    {
        return position;
    }

    // For lookat camera type
    // Sets the distance between the camera and the point it is looking at
    void setDistance(float lookAtDistance)
    {
        this->lookAtDistance = lookAtDistance;
        updateViewMatrix();
    }

    void translateDistance(float delta)
    {
        if ((double)this->lookAtDistance + delta > 0.1) {
            this->lookAtDistance += delta;
        }
        updateViewMatrix();
    }

    glm::vec3 getDirection()
    {
        return cameraFront;
        //return matrices.view[2];
    }

    void setRotation(glm::vec3 rotation)
    {
        this->rotation = rotation;
        updateViewMatrix();
    }

    void rotate(glm::vec3 delta)
    {
        this->rotation += delta;
        updateViewMatrix();
    }

    void translate(glm::vec3 delta)
    {
        this->position += delta;
        updateViewMatrix();
    }

    void setRotationSpeed(float rotationSpeed)
    {
        this->rotationSpeed = rotationSpeed;
    }

    void setMovementSpeed(float movementSpeed)
    {
        this->movementSpeed = movementSpeed;
    }

    void update(float deltaTime)
    {
        if (rotation.x > 89.0f) {
            rotation.x = 89.0f;
        }
        if (rotation.x < -89.0f) {
            rotation.x = -89.0f;
        }
        if (glm::abs(rotation.y) > 359.0f) {
            rotation.y = 0.0f;
        }

        if (type == CameraType::firstperson)
        {
            cameraFront.x = cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
            cameraFront.y = sin(glm::radians(rotation.x));
            cameraFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
            cameraFront = glm::normalize(cameraFront);

            float moveSpeed = deltaTime * (keys.shift ? movementSpeed * 2.0f : movementSpeed);

            if (keys.up) {
                position += cameraFront * moveSpeed;
            }
            if (keys.down) {
                position -= cameraFront * moveSpeed;
            }
            if (keys.left) {
                position += glm::normalize(glm::cross(cameraFront, worldUp)) * moveSpeed;
            }
            if (keys.right) {
                position -= glm::normalize(glm::cross(cameraFront, worldUp)) * moveSpeed;
            }

            updateViewMatrix();
        }
        else {
            updateViewMatrix();
        }
    };
};