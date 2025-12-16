#include <emscripten/bind.h>
#include <vector>
#include <cmath>

using namespace emscripten;

// 1. パーティクル（点）
struct Particle {
    float x, y;          // 現在位置
    float old_x, old_y;  // 1フレーム前の位置（速度計算用）
    float ax, ay;        // 加速度

    // コンストラクタ
    Particle(float startX, float startY) {
        x = old_x = startX;
        y = old_y = startY;
        ax = ay = 0.0f;
    }
};

// 2. 距離拘束（棒）
struct DistanceConstraint {
    int p1_index; // 繋ぐ点の番号
    int p2_index;
    float length; // 守るべき距離

    DistanceConstraint(int idx1, int idx2, float len) 
        : p1_index(idx1), p2_index(idx2), length(len) {}
};

// 3. 世界（シミュレーション全体）
class World {
public:
    std::vector<Particle> particles;
    std::vector<DistanceConstraint> constraints;
    float gravity = 0.5f;

    World() {
        // 初期化：2つの点を作る
        // 点0: 固定用（空中に浮く）
        particles.push_back(Particle(400, 100));
        // 点1: ぶら下がる用
        particles.push_back(Particle(500, 200));

        // 拘束：点0と点1を繋ぐ（距離150くらい）
        float dist = std::hypot(particles[1].x - particles[0].x, particles[1].y - particles[0].y);
        constraints.push_back(DistanceConstraint(0, 1, dist));
    }

    // JSから毎フレーム呼ばれる関数
    void update() {
        // A. 力の適用と移動 (Verlet Integration)
        for (auto& p : particles) {
            // 固定点（点0）は動かさない特別ルール
            if (&p == &particles[0]) continue; 

            float vx = p.x - p.old_x;
            float vy = p.y - p.old_y;

            p.old_x = p.x;
            p.old_y = p.y;

            // 重力を加える
            p.ay = gravity;

            // 位置更新: x = x + v + a
            p.x += vx + p.ax;
            p.y += vy + p.ay;
        }

        // B. 拘束の解決 (PBDの心臓部)
        // ここを何回も回すと「硬い棒」になる
        for (int i = 0; i < 5; i++) {
            for (auto& c : constraints) {
                Particle& p1 = particles[c.p1_index];
                Particle& p2 = particles[c.p2_index];

                float dx = p2.x - p1.x;
                float dy = p2.y - p1.y;
                float dist = std::sqrt(dx*dx + dy*dy);
                
                if (dist == 0) continue; // ゼロ除算防止

                // どれくらいズレてる？ (正なら伸びすぎ、負なら縮みすぎ)
                float diff = (dist - c.length) / dist;

                // 修正ベクトル（それぞれ半分ずつ寄せる）
                float moveX = dx * diff * 0.5f;
                float moveY = dy * diff * 0.5f;

                // 点0は固定なので動かさない
                // 点1だけ動かす
                 // (本来は質量に応じて分配しますが、今は簡易実装で点0固定前提)
                p2.x -= moveX * 2.0f; // 点0が動かない分、点1が全部動く
                p2.y -= moveY * 2.0f;
            }
        }
    }
    void setParticlePos(int index, float x, float y) {
        // 範囲チェック（念のため）
        if (index >= 0 && index < particles.size()) {
            particles[index].x = x;
            particles[index].y = y;
            // 速度をリセットしないと、離した瞬間に吹っ飛ぶのを防ぐため
            // old_x も更新して「止まっている」ことにするテクニック
            particles[index].old_x = x; 
            particles[index].old_y = y;
            // 加速度もリセット
            particles[index].ax = 0;
            particles[index].ay = 0;
        }
    }

    // JSにデータを渡すためのゲッター
    float getParticleX(int index) { return particles[index].x; }
    float getParticleY(int index) { return particles[index].y; }
};

// Emscriptenへの公開設定 (The Glue)
EMSCRIPTEN_BINDINGS(my_module) {
    class_<World>("World")
        .constructor<>()
        .function("update", &World::update)
        .function("getParticleX", &World::getParticleX)
        .function("getParticleY", &World::getParticleY)
        .function("setParticlePos", &World::setParticlePos); // ★ここを追加！
}