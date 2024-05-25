#pragma once

class MovingPlatform;


/// Type ID used by the map files as a numeric identifier.
enum class PlatformPathType: unsigned char {
    Straight,
    StraightContinuous,
    Ellipse,
    Falling,
};


class MovingPlatformPath {
public:
    MovingPlatformPath(float vel, float startX, float startY, float endX, float endY, bool preview);
    virtual ~MovingPlatformPath() = default;

    virtual PlatformPathType typeId() const = 0;
    virtual bool Move(short type) = 0;
    virtual void Reset();

    void SetPlatform(MovingPlatform* platform) {
        pPlatform = platform;
    }

protected:
    MovingPlatform* pPlatform;

    float dVelocity;
    float dVelX[2], dVelY[2];

    float dPathPointX[2], dPathPointY[2];

    float dCurrentX[2], dCurrentY[2];

    friend class MovingPlatform;
    friend class CMap;
    friend void loadcurrentmap();
    friend void insert_platforms_into_map();
};


class StraightPath : public MovingPlatformPath {
public:
    StraightPath(float vel, float startX, float startY, float endX, float endY, bool preview);

    PlatformPathType typeId() const override { return PlatformPathType::Straight; }
    bool Move(short type) override;
    void Reset() override;

protected:
    void SetVelocity(short type);

    short iOnStep[2];
    short iSteps;
    short iGoalPoint[2];

    float dAngle;

    friend class MovingPlatform;
    friend class CMap;
    friend void loadcurrentmap();
    friend void insert_platforms_into_map();
};


class StraightPathContinuous : public StraightPath {
public:
    StraightPathContinuous(float vel, float startX, float startY, float angle, bool preview);

    PlatformPathType typeId() const override { return PlatformPathType::StraightContinuous; }
    bool Move(short type) override;
    void Reset() override;

private:
    float dEdgeX, dEdgeY;

    friend class MovingPlatform;
    friend class CMap;
    friend void loadcurrentmap();
    friend void insert_platforms_into_map();
};


class EllipsePath : public MovingPlatformPath {
public:
    EllipsePath(float vel, float dAngle, float dRadiusX, float dRadiusY, float dCenterX, float dCenterY, bool preview);

    PlatformPathType typeId() const override { return PlatformPathType::Ellipse; }
    bool Move(short type) override;
    void SetPosition(short type);
    void Reset() override;

private:
    float dRadiusX, dRadiusY;
    float dAngle[2], dStartAngle;

    friend class MovingPlatform;
    friend class CMap;
    friend void loadcurrentmap();
    friend void insert_platforms_into_map();
};


class FallingPath : public MovingPlatformPath {
public:
    FallingPath(float startX, float startY);

    PlatformPathType typeId() const override { return PlatformPathType::Falling; }
    bool Move(short type) override;
    void Reset() override;

private:
    friend class MovingPlatform;
};
