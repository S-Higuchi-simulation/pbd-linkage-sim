#include <emscripten/bind.h>
#include <vector>
#include <cmath>

using namespace emscripten;

// 1. パーティクル
struct Particle {
    float x, y;
    float old_x, old_y;
    bool isFixed; // ★追加: 固定されているかどうかのフラグ

    Particle(float startX, float startY, bool fixed = false) {
        x = old_x = startX;
        y = old_y = startY;
        isFixed = fixed;
    }
};

// 2. 距離拘束
struct DistanceConstraint {
    int p1_index;
    int p2_index;
    float length;

    DistanceConstraint(int idx1, int idx2, float len) 
        : p1_index(idx1), p2_index(idx2), length(len) {}
};

// 3. 世界
class World {
public:
    std::vector<Particle> particles;
    std::vector<DistanceConstraint> constraints;
    float gravity = 0.5f; // 重力復活！

    World() {
        // 初期状態は空っぽにしておく（エディタなので）
    }

    // ★追加: JSから点を追加する関数
    int addParticle(float x, float y, bool isFixed) {
        particles.push_back(Particle(x, y, isFixed));
        return particles.size() - 1; // 追加した点のインデックスを返す
    }

    // ★追加: JSから棒を追加する関数
    void addConstraint(int i1, int i2) {
        // 現在の距離を計算して、その長さを維持するようにする
        float dx = particles[i2].x - particles[i1].x;
        float dy = particles[i2].y - particles[i1].y;
        float dist = std::sqrt(dx*dx + dy*dy);
        constraints.push_back(DistanceConstraint(i1, i2, dist));
    }

    // ★追加: 固定状態の切り替え
    void toggleFixed(int index) {
        if(index >= 0 && index < particles.size()) {
            particles[index].isFixed = !particles[index].isFixed;
        }
    }

    // ★追加: 全部消す（リセット用）
    void clear() {
        particles.clear();
        constraints.clear();
    }

    void update() {
        // --- A. 物理計算 ---
        for (auto& p : particles) {
            if (p.isFixed) continue; // ★フラグを見て判定

            float vx = p.x - p.old_x;
            float vy = p.y - p.old_y;
            p.old_x = p.x;
            p.old_y = p.y;
            
            // 簡易摩擦 (0.99を掛けることで空気抵抗っぽくなる)
            p.x += vx * 0.99f;
            p.y += vy * 0.99f + gravity;
        }

        // --- B. 拘束解決 ---
        for (int i = 0; i < 5; i++) { 
            for (auto& c : constraints) {
                Particle& p1 = particles[c.p1_index];
                Particle& p2 = particles[c.p2_index];
                
                float dx = p2.x - p1.x;
                float dy = p2.y - p1.y;
                float dist = std::sqrt(dx*dx + dy*dy);
                if (dist == 0) continue;
                
                float diff = (dist - c.length) / dist;
                float moveX = dx * diff * 0.5f;
                float moveY = dy * diff * 0.5f;

                if (!p1.isFixed && !p2.isFixed) {
                    p1.x += moveX; p1.y += moveY;
                    p2.x -= moveX; p2.y -= moveY;
                } else if (p1.isFixed && !p2.isFixed) {
                    p2.x -= moveX * 2.0f; p2.y -= moveY * 2.0f;
                } else if (!p1.isFixed && p2.isFixed) {
                    p1.x += moveX * 2.0f; p1.y += moveY * 2.0f;
                }
            }
        }
        
        // --- C. 床の衝突判定（簡易） ---
        // これがないと点が無限に落ちていくので追加
        for (auto& p : particles) {
            if (!p.isFixed && p.y > 600) {
                p.y = 600;
                p.old_y = p.y; // 止める
            }
        }
    }

    // ゲッター類
    int getParticleCount() const { return particles.size(); }
    bool isParticleFixed(int index) { return particles[index].isFixed; } // ★追加
    int getConstraintCount() const { return constraints.size(); }
    int getConstraintP1(int i) { return constraints[i].p1_index; }
    int getConstraintP2(int i) { return constraints[i].p2_index; }
    float getParticleX(int index) { return particles[index].x; }
    float getParticleY(int index) { return particles[index].y; }
    
    // マウスドラッグ用
    void setParticlePos(int index, float x, float y) { 
        if (index >= 0 && index < particles.size()) {
            particles[index].x = x; particles[index].y = y;
            particles[index].old_x = x; particles[index].old_y = y; // 速度リセット
        }
    }
};

EMSCRIPTEN_BINDINGS(my_module) {
    class_<World>("World")
        .constructor<>()
        .function("update", &World::update)
        .function("addParticle", &World::addParticle)     // ★追加
        .function("addConstraint", &World::addConstraint) // ★追加
        .function("toggleFixed", &World::toggleFixed)     // ★追加
        .function("clear", &World::clear)                 // ★追加
        .function("isParticleFixed", &World::isParticleFixed) // ★追加
        .function("getParticleCount", &World::getParticleCount)
        .function("getConstraintCount", &World::getConstraintCount)
        .function("getConstraintP1", &World::getConstraintP1)
        .function("getConstraintP2", &World::getConstraintP2)
        .function("getParticleX", &World::getParticleX)
        .function("getParticleY", &World::getParticleY)
        .function("setParticlePos", &World::setParticlePos);
}