#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>
#include <iostream>
#include <math.h>
#include <algorithm>
#include <mutex>
#include <chrono>
#include <thread>
#include <vector>


struct Vector
{
    float x;
    float y;
};

enum class ShapeType
{
    Circle,
    AABB
};

struct Circle
{
    float mass;
    float radius;
    float restitution;
    Vector Vel = {0,0};
    Vector position;
};

struct AABB
{
    Vector min;
    Vector max;
    Vector Vel = {0,0};
    float mass;
    float restitution;
};

struct Object 
{
    ShapeType shapeType;
    union {
        Circle circle;
        AABB aabb;
    };
    
    Object(Circle c) : shapeType(ShapeType::Circle), circle(c) {}

    Object(AABB a) : shapeType(ShapeType::AABB), aabb(a) {}
};

struct Manifold
{
    Object& A;
    Object& B;
    Vector normal;
    float penetration;
    
};

void renderObjects(sf::RenderWindow& window, std::vector<Object>& objects)
{
    window.clear(sf::Color::White);
    for (const auto& obj : objects)
    {
        if (obj.shapeType == ShapeType::AABB)
        {
            sf::RectangleShape aabb(sf::Vector2f(
                obj.aabb.max.x - obj.aabb.min.x,
                obj.aabb.max.y - obj.aabb.min.y
            ));
            aabb.setPosition(sf::Vector2f(obj.aabb.min.x, obj.aabb.min.y));
            aabb.setFillColor(sf::Color::Red);
            window.draw(aabb);
        }
        else if (obj.shapeType == ShapeType::Circle)
        {
            sf::CircleShape circle(obj.circle.radius);
            circle.setPosition(sf::Vector2f(obj.circle.position.x - obj.circle.radius, obj.circle.position.y - obj.circle.radius));
            circle.setFillColor(sf::Color::Magenta);
            window.draw(circle);
        }
    }
    window.display();
}

Manifold getInfoCirclevsAABB(Object& a , Object& b)
{
    float closestX = std::max(b.aabb.min.x , std::min(a.circle.position.x, b.aabb.max.x));
    float closestY = std::max(b.aabb.min.y , std::min(a.circle.position.y, b.aabb.max.y));

    float normalX = a.circle.position.x - closestX;
    float normalY = a.circle.position.y - closestY;

    float len = std::sqrt(normalX * normalX + normalY * normalY);
    Vector normal;
    if(len != 0){
        normalX /= len;
        normalY /= len;
        normal = {normalX, normalY};
    } else {
        normal = {1,0};
    }

    float penetrationDepth = a.circle.radius - len;
    Manifold result = {a, b, normal, penetrationDepth};
    return result;
}

Manifold getInfoCirclevsCircle(Object& a, Object& b)
{
    float netX = a.circle.position.x - b.circle.position.x;
    float netY = a.circle.position.y - b.circle.position.y;
    float div = std::sqrt(netX * netX + netY * netY);
    Vector normal = {netX/div, netY/div};
    float penetrationDepth = (a.circle.radius + b.circle.radius) - div;
    Manifold result = {a, b, normal, penetrationDepth};;
    return result;
    
}

Manifold getInfoAABBvsAABB(Object& a, Object& b)
{  
    Vector normal = {1, 0};  // Default to a valid normal
    float penetrationDepth = 0;
    AABB abox = a.aabb;
    AABB bbox = b.aabb;
    
    Vector n;
    n.x = (b.aabb.max.x + b.aabb.min.x)/2 - (a.aabb.max.x + a.aabb.min.x)/2;
    n.y = (b.aabb.max.y + b.aabb.min.y)/2 - (a.aabb.max.y + a.aabb.min.y)/2;
    std::cout << "vector N (" << n.x << ", " << n.y << ")\n";

    float a_extent = (abox.max.x - abox.min.x)/2;
    float b_extent = (bbox.max.x - bbox.min.x)/2;

    float xOverlap = a_extent + b_extent - std::abs(n.x);

    if(xOverlap > 0)
    {
        a_extent = (abox.max.y - abox.min.y)/2;
        b_extent = (bbox.max.y - bbox.min.y)/2;

        float yOverlap = a_extent + b_extent - std::abs(n.y);

        if(yOverlap > 0)
        {
            if(xOverlap < yOverlap || (abox.Vel.y == 0 && bbox.Vel.y == 0))
            {
                if(n.x < 0 ){
                    normal = {-1, 0};
                }
                else{
                    normal = {1,0};
                }
                penetrationDepth = xOverlap;
            }
            else{
                if(n.y < 0){
                    normal = {0,-1};
                }
                else{
                    normal = {0,1};
                }
                penetrationDepth = yOverlap;
            }
        }
    }
    std::cout << "normal x " << normal.x << " normal y " << normal.y << "\n";
    std::cout << "Penetration Depth " << penetrationDepth << "\n";
    Manifold result = {a, b, normal, penetrationDepth};
    return result;
    
}

Manifold getManifold(Object& A, Object& B)
{  
    switch(A.shapeType){
        case(ShapeType::Circle):
            if(B.shapeType == ShapeType::AABB){
                return getInfoCirclevsAABB(A , B);
            }
            else if(B.shapeType == ShapeType::Circle){
                return getInfoCirclevsCircle(A, B);
            }
            else{
                break;
            }
        case(ShapeType::AABB):
            if(B.shapeType == ShapeType::AABB){
                return getInfoAABBvsAABB(A, B);
            }
            else if(B.shapeType == ShapeType::Circle){
                return getInfoCirclevsAABB(B, A);
                }
            else{
                break;
            }
        default:
            throw std::runtime_error("Unknown shape type");
    }
}

bool CollisionDetectionAABBvsCircle(AABB a, Circle b)
{
    Vector aabb_center = { (a.min.x + a.max.x) / 2, (a.min.y + a.max.y) / 2 };
    Vector distance = { b.position.x - aabb_center.x, b.position.y - aabb_center.y };
    std::cout << "circle position " << b.position.x << ", " << b.position.y << "\n";
    std::cout << " aabb center " << aabb_center.x << ", " << aabb_center.y << "\n";
    //std::cout << "distance " << distance.x + b.radius << ", " << distance.y +b.radius << "\n";
    float x_extent = (a.max.x - a.min.x) / 2;
    float y_extent = (a.max.y - a.min.y) / 2;

    Vector clamp_dist;
    clamp_dist.x = std::clamp(distance.x, -x_extent, x_extent);
    clamp_dist.y = std::clamp(distance.y, -y_extent, y_extent);
    //std::cout << "clamp dist " << clamp_dist.x << ", " << clamp_dist.y << "\n";

    Vector closest;
    closest.x = aabb_center.x + clamp_dist.x;
    closest.y = aabb_center.y + clamp_dist.y;

    //std::cout << "closest point " << closest.x << ", " << closest.y << "\n";
    //std::cout << "aab center y + clamp distance y " << aabb_center.x <<  ", " << clamp_dist.x << "\n";

    Vector circle_center_to_closest = { closest.x - b.position.x, closest.y - b.position.y };
    std::cout << "closest center " << circle_center_to_closest.x << ", " << circle_center_to_closest.y << "\n";

    if ((circle_center_to_closest.x * circle_center_to_closest.x + circle_center_to_closest.y * circle_center_to_closest.y) <= (b.radius * b.radius))
    {
        
        std::cout << "collided\n";
        return true;
    }
    return false;
}

bool CollisionDetectionCirclevsCircle(Circle a, Circle b)
{
    float r = a.radius + b.radius;
    r *= r;
    float xPosition = a.position.x - b.position.x;
    float yPosition = a.position.y - b.position.y;
    return r >= (xPosition * xPosition) + (yPosition * yPosition);
}

bool CollisionDetectionAABBvsAABB(AABB a, AABB b)
{
    if(a.max.x < b.min.x || a.min.x > b.max.x) return false;
    if(a.max.y < b.min.y || a.min.y > b.max.y) return false;
    return true;
}

bool checkShapeCollision(Object A, Object B)
{
    switch(A.shapeType){
        case(ShapeType::Circle):
            if(B.shapeType == ShapeType::AABB){
                return CollisionDetectionAABBvsCircle(B.aabb, A.circle);
            }
            else if(B.shapeType == ShapeType::Circle){
                return CollisionDetectionCirclevsCircle(A.circle, B.circle);
            }
            break;
        case(ShapeType::AABB):
            if(B.shapeType == ShapeType::AABB){
                return CollisionDetectionAABBvsAABB(A.aabb, B.aabb);
            }
            else if(B.shapeType == ShapeType::Circle){
                return CollisionDetectionAABBvsCircle(A.aabb, B.circle);
                }
    }
    return false;
}

Vector relativeVelocity(Vector A, Vector B)
{
    Vector relative;
    relative.x = B.x - A.x;
    relative.y = B.y - A.y;
    return relative;
}

float dotProduct(Vector A, Vector B)
{
    float X = A.x * B.x;
    float Y = A.y * B.y;
    return (X + Y);

}

void PositionCorrection(Manifold m)
{
    const float percent = 0.8f;
    Vector correction;
    correction.x = m.penetration / (m.A.aabb.mass + m.B.aabb.mass) * percent * m.normal.x;
    correction.y = m.penetration / (m.A.aabb.mass + m.B.aabb.mass) * percent * m.normal.y;
    std::cout << "correction x " << correction.x << " correction y " << correction.y << "\n";
    m.A.aabb.max.x -= 1 / m.A.aabb.mass * correction.x;
    m.A.aabb.min.x -= 1 / m.A.aabb.mass * correction.x;
    m.A.aabb.max.y -= 1 / m.A.aabb.mass * correction.y;
    m.A.aabb.min.y -= 1 / m.A.aabb.mass * correction.y;

    m.B.aabb.max.x += 1 / m.B.aabb.mass * correction.x;
    m.B.aabb.min.x += 1 / m.B.aabb.mass * correction.x;
    m.B.aabb.max.y += 1 / m.B.aabb.mass * correction.y;
    m.B.aabb.min.y += 1 / m.B.aabb.mass * correction.y;
}


void collisionResolutionCirclevsAABB(Manifold m)
{
    Vector RV = relativeVelocity(m.A.circle.Vel, m.B.aabb.Vel);

    Vector normal = m.normal;

    float velAlongNormal = dotProduct(RV, normal);
    std::cout << "Rv x " << RV.x << " RV y " << RV.y << "\n";
    std::cout << "normal x " << normal.x << " normal y " << normal.y << "\n";
    std::cout << "vel Along Normal " << velAlongNormal << "\n";
    if (velAlongNormal > 0) {
        return;
    }

    float e = std::min(m.A.circle.restitution, m.B.aabb.restitution);

    float j = -(1 + e) * velAlongNormal;

    float mass_sum = (1 / m.A.aabb.mass + 1 / m.B.aabb.mass);

    j /= mass_sum;

    Vector impulse;
    impulse.x = j * normal.x;
    impulse.y = j * normal.y;
    std::cout << "impulse x " << impulse.x << " impulse y " << impulse.y << "\n";

    if (m.A.circle.mass > 0) {
        m.A.aabb.Vel.x -= 1 / m.A.aabb.mass * impulse.x;
        m.A.aabb.Vel.y -= 1 / m.A.aabb.mass * impulse.y;
    }

    if (m.B.circle.mass > 0) {
        m.B.circle.Vel.x += (1 / m.B.circle.mass) * impulse.x;
        m.B.circle.Vel.y += (1 / m.B.circle.mass) * impulse.y;
    }
    std::cout << "A vel (" << m.A.aabb.Vel.x << ", " << m.A.aabb.Vel.y << ")\n";
    std::cout << "B vel (" << m.B.aabb.Vel.x << ", " << m.B.aabb.Vel.y << ")\n";

    PositionCorrection(m);
}

void collisionResolutionCirclevsCircle(Manifold m)
{
    return;
}

void collisionResolutionAABBvsAABB(Manifold m)
{
    
    Vector RV = relativeVelocity(m.A.aabb.Vel, m.B.aabb.Vel);

    
    Vector normal = m.normal;

    float velAlongNormal = dotProduct(RV, normal);
    std::cout << "Rv x " << RV.x << " RV y " << RV.y << "\n";
    std::cout << "normal x " << normal.x << " normal y " << normal.y << "\n";
    std::cout << "vel Along Normal " << velAlongNormal << "\n";
    if(velAlongNormal > 0){
        return;
    }

    float e = std::min(m.A.aabb.restitution, m.B.aabb.restitution);

    float j = -(1 + e) * velAlongNormal;
    
    float mass_sum = (1/m.A.aabb.mass + 1/m.B.aabb.mass);
    
    j /= mass_sum;

    Vector impulse;
    impulse.x = j * normal.x;
    impulse.y = j * normal.y;
    std::cout << "impulse x " << impulse.x << " impulse y " << impulse.y << "\n";
    if (m.A.aabb.mass > 0) {
        m.A.aabb.Vel.x -= 1 / m.A.aabb.mass * impulse.x;
        m.A.aabb.Vel.y -= 1 / m.A.aabb.mass * impulse.y;
    }
    
    if (m.B.aabb.mass > 0) {
        m.B.aabb.Vel.x += (1 / m.B.aabb.mass) * impulse.x;
        m.B.aabb.Vel.y += (1 / m.B.aabb.mass) * impulse.y;
    }
    std::cout << "A vel (" << m.A.aabb.Vel.x << ", " << m.A.aabb.Vel.y << ")\n";
    std::cout << "B vel (" << m.B.aabb.Vel.x << ", " << m.B.aabb.Vel.y << ")\n";

    PositionCorrection(m);
}




void resolveColission(Manifold m)
{
    switch(m.A.shapeType){
        case(ShapeType::Circle):
            if(m.B.shapeType == ShapeType::AABB){
                collisionResolutionCirclevsAABB(m);
                return;
            }
            else if(m.B.shapeType == ShapeType::Circle){
                collisionResolutionCirclevsCircle(m);
                return;
            }
            return;
        case(ShapeType::AABB):
            if(m.B.shapeType == ShapeType::AABB){
                
                collisionResolutionAABBvsAABB(m);
                return;
            }
            else if(m.B.shapeType == ShapeType::Circle){
                collisionResolutionCirclevsAABB(m);
                return;
                }

             return;
    }
}

void collisionDetection(std::vector<Object>& objects, bool& running, std::mutex& objMutex)
{
    while(running){
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
       // std::cout << "collition detection\n";

        for (size_t i = 0; i < objects.size(); i++){
            for (size_t j = i + 1; j < objects.size(); j++){
                if(checkShapeCollision(objects[i], objects[j])){ 
                    //std::cout << "colided\n";
                    std::lock_guard<std::mutex> lock(objMutex);
                    resolveColission(getManifold(objects[i], objects[j]));
                }
            }
        }
    }
    
}

void moveObjects(std::vector<Object>& objects, bool& running, std::mutex& objMutex)
{
    while(running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        //std::cout << "Moving\n";
        std::lock_guard<std::mutex> lock(objMutex);

        for(auto& obj : objects){
            if(obj.shapeType == ShapeType::Circle){
            obj.circle.position.x += obj.circle.Vel.x;
            obj.circle.position.y += obj.circle.Vel.y;
            }
            else {
                obj.aabb.min.x += obj.aabb.Vel.x;
                obj.aabb.max.x += obj.aabb.Vel.x;
                obj.aabb.min.y += obj.aabb.Vel.y;
                obj.aabb.max.y += obj.aabb.Vel.y;
            }
        }
    }
    
}


int main()
{   
    sf::RenderWindow window(sf::VideoMode({ 1000,1000 }), "Physics Engine");

    bool running = true;

    std:: mutex objMutex;

    std::vector<Object> objects = {
              //Object(AABB{{40, 50}, {200, 240}, {.9f, .3}, 10, 0.8f}), // AABB
              Object(AABB{{450, 100}, {550, 200}, {0, 0}, 5, 0.6f}), // AABB
              //Object(AABB{{40, 300}, {200, 500}, {2.0f, .3}, 4, 0.8f}), // AABB
              //Object(AABB{{800, 300}, {950, 500}, {-10.0f, 0}, 5, 0.6f}), // AABB
              //Object(AABB{{40, 50}, {200, 240}, {.9f, .3}, 10, 0.8f}), // AABB
              //Object(AABB{{280, 40}, {400, 240}, {0, 0}, 5, 0.6f}), // AABB
              Object(Circle{5, 50, .8f, {2.0f, 0}, {300, 150}}) //circle
    };
    std::vector<Object> renderCopy;

    std::thread moveThread;
    std::thread collisionThread;

    
    
    moveThread = std::thread(moveObjects, std::ref(objects), std::ref(running), std::ref(objMutex));
    collisionThread = std::thread(collisionDetection, std::ref(objects), std::ref(running), std::ref(objMutex));

    auto start_time = std::chrono::high_resolution_clock::now();

    while (window.isOpen())
    {
        //while (const std::optional event = window.pollEvent()) 
        //{
            //if (event->is<sf::Event::Closed>()) {
                //window.close();
            //}
            auto frameStart = std::chrono::high_resolution_clock::now();
            std::chrono::seconds duration(8);  

            {
                //std::cout << "rendering trying to aquire lock\n";
                std::lock_guard<std::mutex> lock(objMutex);
                //std::cout << "rendering aquired lock\n";
                renderCopy = objects;
              
           }
            window.clear(sf::Color::White);  // Clear window before each render

            renderObjects(window, renderCopy);  // Render objects
            auto end_time = std::chrono::high_resolution_clock::now();

           auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
           //std::cout << "rendered\n";
           if (elapsed_time >= std::chrono::seconds(5))
           {
               window.close();
               running = false;
           }
           auto frameEnd = std::chrono::high_resolution_clock::now();
           std::this_thread::sleep_for(std::chrono::milliseconds(16) - (frameEnd - frameStart));
        

        
    }
    
    
    
    if (moveThread.joinable()) moveThread.join();
    if (collisionThread.joinable()) collisionThread.join();
       
    for (const auto& obj : objects) {
        if (obj.shapeType == ShapeType::Circle) {
            std::cout << "Circle final pos: " << obj.circle.position.x << " " << obj.circle.position.y << "\n";
        } else if (obj.shapeType == ShapeType::AABB) {
            std::cout << "AABB final pos: (" << obj.aabb.min.x << ", " << obj.aabb.min.y << "), (" << obj.aabb.max.x << ", " << obj.aabb.max.y << ")\n";
            std::cout << "AABB final velocity: (" << obj.aabb.Vel.x << ", " << obj.aabb.Vel.y << ")\n";
        }
    }
    return 0;
}