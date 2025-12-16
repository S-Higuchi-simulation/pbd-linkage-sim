#include <emscripten/bind.h>
#include <vector>
#include <cmath>

using namespace emscripten;

// 1. パーティクル（点）
struct Particle {
    float x, y;
    float old_x, old_y;
    float ax, ay;

    Particle(float startX, float startY) {
        x = old_x = startX;
        y = old_y = startY;
        ax = ay = 0.0f;
    }
};

// 2. 距離拘束（棒）
struct DistanceConstraint {
    int p1_index;
    int p2_index;
    float length;

    DistanceConstraint(int idx1, int idx2, float len) 
        : p1_index(idx1), p2_index(idx2), length(len) {}
};

// 3. 世界（シミュレーション全体）
class World {
public:
    std::vector<Particle> particles;
    std::vector<DistanceConstraint> constraints;
    float gravity = 0.0f; // 重力OFF（機構の動きを見るため）
    float time = 0.0f;

    World() {
        // --- 4節リンク機構 (修正版: 正しい構成) ---
        
        // 0. 固定点（モーターの回転中心）
        particles.push_back(Particle(300, 300));

        // 1. クランクの先端（モーターで回される点）
        particles.push_back(Particle(300 + 50, 300)); // クランク長 50

        // 2. 中間リンクとレバーの接続点
        particles.push_back(Particle(500, 200));

        // 3. 固定点（レバーの回転中心）
        particles.push_back(Particle(550, 400)); 

        // --- 拘束（棒）の追加 ---
        // ※点0と1はモーター駆動なので棒（距離拘束）は不要
        
        // 接続ロッド (1-2)
        constraints.push_back(DistanceConstraint(1, 2, 200));
        
        // 揺動レバー (2-3)
        constraints.push_back(DistanceConstraint(2, 3, 200));
    }

    void update() {
        time += 0.05f;

        // --- A. モーター制御 ---
        float crankRadius = 50.0f;
        float centerX = particles[0].x;
        float centerY = particles[0].y;
        
        // 点1を強制的に回す
        particles[1].x = centerX + std::cos(time) * crankRadius;
        particles[1].y = centerY + std::sin(time) * crankRadius;
        particles[1].old_x = particles[1].x;
        particles[1].old_y = particles[1].y;

        // --- B. 物理計算 ---
        for (auto& p : particles) {
            // 固定点(0, 3)と、モーター点(1)は物理計算から除外
            if (&p == &particles[0] || &p == &particles[3] || &p == &particles[1]) continue;

            float vx = p.x - p.old_x;
            float vy = p.y - p.old_y;
            p.old_x = p.x;
            p.old_y = p.y;
            p.x += vx;
            p.y += vy + gravity;
        }

        // --- C. 拘束解決 ---
        for (int i = 0; i < 20; i++) { 
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

                // 固定点の判定
                bool p1_fixed = (&p1 == &particles[0] || &p1 == &particles[3] || &p1 == &particles[1]);
                bool p2_fixed = (&p2 == &particles[0] || &p2 == &particles[3] || &p2 == &particles[1]);

                if (!p1_fixed && !p2_fixed) {
                    p1.x += moveX; p1.y += moveY;
                    p2.x -= moveX; p2.y -= moveY;
                } else if (p1_fixed && !p2_fixed) {
                    p2.x -= moveX * 2.0f; p2.y -= moveY * 2.0f;
                } else if (!p1_fixed && p2_fixed) {
                    p1.x += moveX * 2.0f; p1.y += moveY * 2.0f;
                }
            }
        }
    }

    // --- D. 自由度計算 ---
    int getDOF() {
        return 1; // 簡易実装
    }
    
    // JS連携用ヘルパー関数
    int getParticleCount() const { return particles.size(); }
    int getConstraintCount() const { return constraints.size(); }
    
    // ★ここが大事！JSから個別の拘束情報にアクセスするための関数
    int getConstraintP1(int i) { return constraints[i].p1_index; }
    int getConstraintP2(int i) { return constraints[i].p2_index; }

    float getParticleX(int index) { return particles[index].x; }
    float getParticleY(int index) { return particles[index].y; }
    
    void setParticlePos(int index, float x, float y) { 
        if (index >= 0 && index < particles.size()) {
            particles[index].x = x; particles[index].y = y;
            particles[index].old_x = x; particles[index].old_y = y;
        }
    }
};

// Emscriptenへの公開設定（ここに書き忘れるとエラーになります！）
EMSCRIPTEN_BINDINGS(my_module) {
    class_<World>("World")
        .constructor<>()
        .function("update", &World::update)
        .function("getDOF", &World::getDOF)
        .function("getParticleCount", &World::getParticleCount)
        .function("getConstraintCount", &World::getConstraintCount)
        .function("getConstraintP1", &World::getConstraintP1) // ★追加
        .function("getConstraintP2", &World::getConstraintP2) // ★追加
        .function("getParticleX", &World::getParticleX)
        .function("getParticleY", &World::getParticleY)
        .function("setParticlePos", &World::setParticlePos);
}