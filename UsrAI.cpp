#include "UsrAI.h"
#include<set>
#include <iostream>
#include<unordered_map>
#include<list>
#include <cstdlib>

using namespace std;
tagGame tagUsrGame;
ins UsrIns;
/*##########DO NOT MODIFY THE CODE ABOVE##########*/

static bool towerTargetInRange(int towerDR, int towerUR, int objDR, int objUR)
{
    const int range = static_cast<int>(DIS_ARROWTOWER);
    return abs(objDR - towerDR) <= range && abs(objUR - towerUR) <= range;
}
//--------------------敌袭应对--------------------
//全局计时器
static int timer = 0;
static bool attackInComing = false;
static int chariotCnt = 0;
static int chariotArcherCnt = 0;
static int coolDownWhenAttacked = 0;
static bool goHome = false;
static int priestRetreatTimer = 0;
static int priestConvertTimer = 0;
static int priestIdleMoveTimer = 0;              // 祭司闲时"回营地"移动节流(每25帧一次)
static unordered_map<int, int> allOut_RangedLock;   // 远程兵(SN)当前锁定的目标SN(锁定狙击用)
//--------------------祭司探图--------------------
//大本营坐标
static bool baseFound = false;
static int baseSN = -1;
static int baseBlockDR = 50;
static int baseBlockUR = 50;
//探图祭祀信息
static bool priestFound = false;
static int priestSN = -1;
static double priestDR = 50;
static double priestUR = 50;
static int priestBlockDR = 50;
static int priestBlockUR = 50;
static double preDR = -1;
static double preUR = -1;
static bool canConvert = false;
static bool isStill = false;
static int recordIntervalTimer = 0;
static int priestState = -1;

static int priestExplore_StuckCnt = 0;      // 连续卡住次数,用来强制换目标
static double priestExplore_PreDR = -1;     // 卡住检测:上一帧祭司位置
static double priestExplore_PreUR = -1;
static int priestExplore_StillTimer = 0;    // 卡住检测计时器
//探图命令
static bool needNewTarget = true;
static int coolDown = 0;
static bool goingRight = true;
static int targetBlockDR = 50;
static int targetBlockUR = 50;
static int URMaxCnt = 0;
//探图完成判断
static bool needExploration = true;
static bool explorationFinished = false;
static int totalBlocks = 10000;
static double exploredRatio = 0;
//--------------------农民工作--------------------
static bool isInitializing = true;
static vector<int>berryFarmers;
static vector<int>woodFarmers;
static vector<int>stoneFarmers;
static vector<int>buildingFarmers;
static vector<int>hunterFarmers;
static vector<int>farmFarmers;
//--------------------建筑工作--------------------
static bool arrowTowerResearching = false;
static int arrowTowerResearchTimer = 0;
static bool arrowTowerUnlocked = false;
static bool wheelResearching = false;
static int wheelResearchTimer = 0;
static bool wheelUnlocked = false;
static bool logisticsResearching=false;
//--------------------发动总攻--------------------
static int allOut_Tick = 0;                        // 节流计数器(每25帧处理一次)
static bool allOut_Started = false;                // 18 分钟到了,总攻开始
static bool allOut_CampFounded = false;            // 营地位置已确定
static int allOut_CampDR = -1;                     // 营地中心块坐标
static int allOut_CampUR = -1;
static bool allOut_EnemyCleared = false;           // 敌方武装+农民已清空
static unordered_map<int, pair<int, int>> allOut_Targets;  // 每个兵的探索目标
static const int ALLOUT_TRIGGER_TICK = 27000;   // 18 分钟(按25 tick≈1秒换算,观感不对就改这个数)
static const int ALLOUT_CHECK_EVERY = 25;          // 每25帧检查/发令一次
static bool allOut_EnemySpotted = false;           // 已发现敌人营地/敌人
static int  allOut_EnemySpotDR = -1;               // 敌人参考点(块坐标)
static int  allOut_EnemySpotUR = -1;
static unordered_map<int, int> allOut_Dir;         // 每个兵当前探索方向(0=东 1=南 2=西 3=北)
static bool allOut_Gathered = false;
static bool allOut_AttackEnabled = false;          // 总人口>=50 才允许正式攻打(一次性)

//--------------------小小功能--------------------
int getBuildingSize(int type) {
    switch (type) {
    case BUILDING_HOME: return 2;
    case BUILDING_GRANARY: return 3;
    case BUILDING_STOCK: return 3;
    case BUILDING_FARM: return 3;
    case BUILDING_ARROWTOWER: return 2;
    case BUILDING_CENTER: return 3;
    case BUILDING_ARMYCAMP: return 3;
    case BUILDING_MARKET: return 3;
    case BUILDING_STABLE: return 3;
    case BUILDING_RANGE: return 3;
    case BUILDING_DOCK: return 3;
    case BUILDING_SIEGE: return 3;
    case BUILDING_COLLAGE: return 3;
    default: return 1;
    }
}

void UsrAI::processData()
{
    /*while(logo<15){
        cheatAction();
        cheatRes();
        logo++;
    }*/
    tagInfo info = getInfo();
    //更新地图信息
    int curMap[100][100] = { 0 };//-1不可达 0可建造 1资源 2建筑 3单位
    if (1) {
        if (info.theMap != nullptr) {
            for (int dr = 0; dr < MAP_L; dr++) {
                for (int ur = 0; ur < MAP_U; ur++) {
                    const tagTerrain& t = (*info.theMap)[dr][ur];
                    if (t.type != (MAPPATTERN_UNKNOWN || MAPPATTERN_OCEAN))curMap[dr][ur] = -1;
                }
            }
        }
        for (tagResource& resource : info.resources) {
            int dr = resource.BlockDR;
            int ur = resource.BlockUR;
            curMap[dr][ur] = 1;
            if (resource.Type == RESOURCE_STONE || RESOURCE_GOLD) {
                curMap[dr + 1][ur] = curMap[dr][ur + 1] = curMap[dr + 1][ur + 1] = 1;
            }
        }
        for (tagBuilding& building : info.buildings) {
            int size = getBuildingSize(building.Type) + 1;
            int dr = building.BlockDR;
            int ur = building.BlockUR;
            for (int i = 0; i < size; i++) {
                for (int j = 0; j < size; j++) {
                    curMap[dr + i][ur + j] = 2;
                }
            }
        }
        auto markUnits = [&](auto& units) {
            for (auto& unit : units) {
                curMap[unit.BlockDR][unit.BlockUR] = 3;
            }
            };
        markUnits(info.farmers);
        markUnits(info.armies);
        markUnits(info.enemy_armies);
        markUnits(info.enemy_farmers);
    }
    //更新祭祀信息
    for (const tagArmy& army : info.armies) {
        if (army.SN == priestSN) {
            priestDR = army.DR;
            priestUR = army.UR;
            priestBlockDR = priestDR / BLOCKSIDELENGTH;
            priestBlockUR = priestUR / BLOCKSIDELENGTH;
            priestState = army.NowState;
            if (army.ConvertCooldown == 0)canConvert = true;
            else canConvert = false;
            break;
        }
    }
    //--------------------敌袭应对--------------------
    if (1) {
        timer++;
        if (timer == 5600) {
            attackInComing = true;
            /*for (tagBuilding& b : info.buildings) {
                if (b.Type != BUILDING_ARROWTOWER)continue;
                HumanMove(priestSN, b.BlockDR * BLOCKSIDELENGTH, b.BlockUR * BLOCKSIDELENGTH);
            }*/
            HumanMove(priestSN, (baseBlockDR - 1) * BLOCKSIDELENGTH, (baseBlockUR - 1) * BLOCKSIDELENGTH);
        }
        auto findBestTargetEnemySN = [&]() {
            int bestTargetEnemySN = -1;
            double closestD = 1e9;
            for (tagArmy& enemy : info.enemy_armies) {
                double d = calDistance(priestDR, priestUR, enemy.DR, enemy.UR);
                if (d < closestD && d < DIS_PRIEST * BLOCKSIDELENGTH) {
                    bestTargetEnemySN = enemy.SN;
                    closestD = d;
                }
            }
            return bestTargetEnemySN;
            };
        bool hasAggro = false;
        for (tagArmy& army : info.armies) {
            if (army.Sort == AT_PRIEST)continue;
            if (army.NowState == HUMAN_STATE_ATTACKING) {
                hasAggro = true;
                break;
            }
        }
        // if ((hasAggro || info.armies.size() < 3) && attackInComing && canConvert && priestState == HUMAN_STATE_IDLE) {
        //     int targetSN = findBestTargetEnemySN();
        //     if (targetSN != -1) {
        //         HumanAction(priestSN, targetSN);
        //     }
        // }
        if (coolDownWhenAttacked > 0)coolDownWhenAttacked--;

        if (attackInComing) {
            auto retreatFree = [&](int dr, int ur) -> bool {
                if (dr < 0 || dr >= MAP_L || ur < 0 || ur >= MAP_U) return false;
                if ((*info.theMap)[dr][ur].type == MAPPATTERN_UNKNOWN) return false;
                if ((*info.theMap)[dr][ur].type == MAPPATTERN_OCEAN) return false;
                for (const tagResource& r : info.resources) {
                    if (r.Type == RESOURCE_TREE && dr == r.BlockDR && ur == r.BlockUR) return false;   // 树占1格不能踩
                }
                return true;
            };
            // ---------- 招降最高优先级:冷却一好,就算在逃/在回家也直接打断去招降 ----------
            bool wantConvert = false;
            if (canConvert) {
                int cSN = -1; double cBest = 1e18; int cDR = -1, cUR = -1;
                for (const tagArmy& e : info.enemy_armies) {
                    double dD = calDistance(priestDR, priestUR, e.DR, e.UR);
                    if (dD < cBest) { cBest = dD; cSN = e.SN; cDR = e.BlockDR; cUR = e.BlockUR; }
                }
                for (const tagFarmer& f : info.enemy_farmers) {
                    double dD = calDistance(priestDR, priestUR, f.DR, f.UR);
                    if (dD < cBest) { cBest = dD; cSN = f.SN; cDR = f.BlockDR; cUR = f.BlockUR; }
                }
                if (cSN != -1) {
    wantConvert = true;
    goHome = false;                       // 掐掉回家念头
    // 施法保护:正在招降动作中绝不再发指令,免得被打断原地重来
    bool converting = (priestState == HUMAN_STATE_ATTACKING
                    || priestState == HUMAN_STATE_WORKING);
    if (!converting) {
        priestConvertTimer++;             // 统一节流:祭司每25帧最多下一条指令
        if (cBest <= DIS_PRIEST * BLOCKSIDELENGTH) {
            if (priestConvertTimer % 25 == 2) {
                HumanAction(priestSN, cSN);   // 射程内招降同样25帧一次,不刷屏
            }
        }
        else {
            // 射程外:打断逃跑/回家,直接追过去再招降(移动每25帧发一次)
            if (priestConvertTimer % 25 == 0) {
                HumanMove(priestSN, cDR * BLOCKSIDELENGTH,
                                 cUR * BLOCKSIDELENGTH);
            }
        }
    }
}
            }
            // ---------- 招降没戏(冷却中/没目标)才轮到逃跑/回家兜底 ----------
            if (!wantConvert) {
                // cooling down: FLEE from nearest enemy; when ready, wantConvert takes over
                int nr=-1, nu=-1; double cD2=1e18;
                for (const tagArmy& e : info.enemy_armies) {
                    double dd = calDistance(priestDR, priestUR, e.DR, e.UR) / BLOCKSIDELENGTH;
                    if (dd < cD2) { cD2 = dd; nr = e.BlockDR; nu = e.BlockUR; }
                }
                for (const tagFarmer& f : info.enemy_farmers) {
                    double dd = calDistance(priestDR, priestUR, f.DR, f.UR) / BLOCKSIDELENGTH;
                    if (dd < cD2) { cD2 = dd; nr = f.BlockDR; nu = f.BlockUR; }
                }
                if (nr != -1 && cD2 < 20) {
                    // cooling + enemy near: run away (throttled)
                    goHome = false;
                    if (priestState != HUMAN_STATE_WALKING) {
                        priestRetreatTimer++;
                        if (priestRetreatTimer % 60 == 1) {
                            int rr = (priestBlockDR > nr) ? 1 : ((priestBlockDR < nr) ? -1 : 0);
                            int uu = (priestBlockUR > nu) ? 1 : ((priestBlockUR < nu) ? -1 : 0);
                            int mdr = priestBlockDR + rr * 4, mur = priestBlockUR + uu * 4;
                            if (!retreatFree(mdr, mur)) { mdr = priestBlockDR + rr * 4; mur = priestBlockUR + uu * 4; }
                            if (retreatFree(mdr, mur)) {
                                HumanMove(priestSN, mdr * BLOCKSIDELENGTH, mur * BLOCKSIDELENGTH);
                            }
                        }
                    }
                }
                else if (priestState == HUMAN_STATE_IDLE) {
                    bool nearBase = abs(priestBlockDR - baseBlockDR) + abs(priestBlockUR - baseBlockUR) <= 4;
                    if (!nearBase && !(allOut_CampFounded && info.enemy_armies.empty() && info.enemy_farmers.empty())) {
                        priestIdleMoveTimer++;
                        if (priestIdleMoveTimer % 25 == 1) {
                            if (allOut_CampFounded) {
                                int hDR = allOut_CampDR - 12, hUR = allOut_CampUR - 3;
                                if (!retreatFree(hDR, hUR)) { hDR = allOut_CampDR + 3; hUR = allOut_CampUR + 12; }
                                if (!retreatFree(hDR, hUR)) { hDR = allOut_CampDR; hUR = allOut_CampUR + 8; }
                                if (retreatFree(hDR, hUR)) {
                                    HumanMove(priestSN, hDR * BLOCKSIDELENGTH, hUR * BLOCKSIDELENGTH);
                                }
                            } else {
                                goHome = true;
                            }
                        }
                    }
                }
            }
        }

        //--------------------------------------------
        // if (attackInComing) {
        //     auto retreatFree = [&](int dr, int ur) -> bool {
        //         if (dr < 0 || dr >= MAP_L || ur < 0 || ur >= MAP_U) return false;
        //         if ((*info.theMap)[dr][ur].type == MAPPATTERN_UNKNOWN) return false;
        //         if ((*info.theMap)[dr][ur].type == MAPPATTERN_OCEAN) return false;
        //         return true;
        //     };
        //     // 本轮是否已经(或即将)发起转化 → 是就绝不去动祭司
        //     bool willConvert = false;
        //     if (canConvert && priestState == HUMAN_STATE_IDLE
        //         && (hasAggro || info.armies.size() < 3)) {
        //         willConvert = (findBestTargetEnemySN() != -1);
        //     }
        //     // 最近敌人距离(格)
        //     int nearDist = 9999;
        //     for (const tagArmy& e : info.enemy_armies) {
        //         double dD = calDistance(priestDR, priestUR, e.DR, e.UR) / BLOCKSIDELENGTH;
        //         if (dD < nearDist) nearDist = (int)dD;
        //     }
        //     bool enemyTouching = nearDist < 10;               // 10 格内才算被贴脸
        //     bool nearBase      = abs(priestBlockDR - baseBlockDR)
        //                        + abs(priestBlockUR - baseBlockUR) <= 4;

        //     if (priestState==HUMAN_STATE_IDLE && !willConvert && enemyTouching) {
        //         // 被贴脸又没法转化:小步后撤(每25帧一次),别硬站挨打
        //         goHome = false;
        //         priestRetreatTimer++;
        //         if (priestRetreatTimer % 25 == 0) {
        //             int bestDR = priestBlockDR, bestUR = priestBlockUR;
        //             int bestD = -1;
        //             for (int dr = priestBlockDR - 10; dr <= priestBlockDR + 10; dr++) {
        //                 for (int ur = priestBlockUR - 10; ur <= priestBlockUR + 10; ur++) {
        //                     if (!retreatFree(dr, ur)) continue;
        //                     int d2 = 9999;
        //                     for (const tagArmy& e : info.enemy_armies) {
        //                         int dd = abs(e.BlockDR - dr) + abs(e.BlockUR - ur);
        //                         if (dd < d2) d2 = dd;
        //                     }
        //                     if (d2 > bestD) { bestD = d2; bestDR = dr; bestUR = ur; }
        //                 }
        //             }
        //             if (bestD > 0) {
        //                 HumanMove(priestSN, bestDR * BLOCKSIDELENGTH,
        //                                  bestUR * BLOCKSIDELENGTH);
        //             }
        //         }
        //     }
        //     else if (!enemyTouching && !nearBase && priestState == HUMAN_STATE_IDLE) {
        //         if (allOut_CampFounded) {
        //             // 营地已建:回的是营地外围,不是基地
        //             int hDR = allOut_CampDR - 12, hUR = allOut_CampUR - 3;
        //             if (!retreatFree(hDR, hUR)) { hDR = allOut_CampDR + 3; hUR = allOut_CampUR + 12; }
        //             HumanMove(priestSN, hDR * BLOCKSIDELENGTH, hUR * BLOCKSIDELENGTH);
        //         }
        //         else {
        //             goHome = true;                    // 真没敌人了才回基地
        //         }
        //     }
        // }

        //--------------------------------------------

        if (!canConvert && coolDownWhenAttacked == 0) {
            coolDownWhenAttacked = 25;
            int bestDR = -1;
            int bestUR = -1;
            double bestScore = -1e9;
            int startDR = max(priestBlockDR - 15, 2);
            int startUR = max(priestBlockUR - 15, 2);
            int endDR = min(priestBlockDR + 15, MAP_L - 2);
            int endUR = min(priestBlockUR + 15, MAP_U - 2);
            for (int dr = startDR; dr <= endDR; dr++) {
                for (int ur = startUR; ur <= endUR; ur++) {
                    double score = 0;
                    int weight = 0;
                    if (curMap[dr][ur] != 0)continue;
                    for (tagArmy& enemy : info.enemy_armies) {
                        double dToEnemy = calDistance(priestDR, priestUR, enemy.DR, enemy.UR);
                        if (dToEnemy > 15 * BLOCKSIDELENGTH)continue;
                        weight++;
                        score += abs(dr - enemy.BlockDR) + abs(ur - enemy.BlockUR);
                    }
                    if (weight == 0 && !(allOut_CampFounded && info.enemy_armies.empty() && info.enemy_farmers.empty()))goHome = true;  // 敌方单位已清时交给祭司转建筑,不回家
                    //score += (20 - abs(dr - baseBlockDR) - abs(ur - baseBlockUR)) * weight;
                    if (score > bestScore) {
                        bestScore = score;
                        bestDR = dr;
                        bestUR = ur;
                    }
                }
            }
            if (bestDR != -1 && bestUR != -1) {
                HumanMove(priestSN, bestDR * BLOCKSIDELENGTH, bestUR * BLOCKSIDELENGTH);
            }
        }
        if (goHome) {
            goHome = false;
            HumanMove(priestSN, (baseBlockDR - 2)* BLOCKSIDELENGTH, (baseBlockUR - 2)* BLOCKSIDELENGTH);
        }
        auto towerAutoAttack = [&](int towerSN, int towerDR, int towerUR, int project) {
            if (project != -1)return;
            int targetSN = -1;
            int blood = 1e9;
            for (tagArmy& enemy : info.enemy_armies) {
                if (!towerTargetInRange(towerDR, towerUR, enemy.BlockDR, enemy.BlockUR))continue;
                if (enemy.Blood < blood) {
                    blood = enemy.Blood;
                    targetSN = enemy.SN;
                }
            }
            if (targetSN != -1) {
                HumanAction(towerSN, targetSN);
            }
            };
        for (tagArmy& army : info.armies) {
            if (army.NowState != HUMAN_STATE_IDLE)continue;
            if (army.Sort == AT_PRIEST)continue;
            if (!allOut_Started && abs(army.BlockDR - baseBlockDR) + abs(army.BlockUR - baseBlockUR) > 20) {
                HumanMove(army.SN, (baseBlockDR - 3)* BLOCKSIDELENGTH, (baseBlockUR - 3)* BLOCKSIDELENGTH);
            }
        }
        for (tagBuilding& b : info.buildings) {
            if (b.Type != BUILDING_ARROWTOWER)continue;
            towerAutoAttack(b.SN, b.BlockDR, b.BlockUR, b.Project);
    }
    
        if(1){
        // ---- 最高优先:正在攻击/锁定祭司的敌人 → 所有空闲兵立刻集火 ----
            for (tagArmy& enemy : info.enemy_armies) {
                if (enemy.WorkObjectSN == priestSN) {      // 该敌人当前在打祭司
                    for (tagArmy& army : info.armies) {
                        if (army.SN == priestSN) continue;
                        if (army.NowState != HUMAN_STATE_IDLE) continue;
                        HumanAction(army.SN, enemy.SN);
                    }
                }
            }
            // ---- 第一优先级:威胁祭司的敌人(15 格内)→ 所有空闲兵立刻去打 ----
            bool priestThreatened = false;
            for (tagArmy& enemy : info.enemy_armies) {
                if (abs(enemy.BlockDR - priestBlockDR) + abs(enemy.BlockUR - priestBlockUR) < 15) {
                    priestThreatened = true;
                    for (tagArmy& army : info.armies) {
                        if (army.SN == priestSN)continue;
                        if (army.NowState != HUMAN_STATE_IDLE)continue;
                        HumanAction(army.SN, enemy.SN);
                    }
                }
            }
            // ---- 第二优先级:祭司周围安全了,才分兵守家(敌人靠近家 30 格) ----
            if (!priestThreatened) {
                for (tagArmy& enemy : info.enemy_armies) {
                    if (abs(enemy.BlockDR - baseBlockDR) + abs(enemy.BlockUR - baseBlockUR) < 30) {
                        for (tagArmy& army : info.armies) {
                            if (army.SN == priestSN)continue;
                            if (army.NowState != HUMAN_STATE_IDLE)continue;
                            HumanAction(army.SN, enemy.SN);
                        }
                    }
                }
            }
        }
    }
    //--------------------祭司探图--------------------
    if (1) {
        // 找大本营
        if (!baseFound) {
            for (const tagBuilding& building : info.buildings) {
                if (building.Type == BUILDING_CENTER) {
                    baseSN = building.SN;
                    baseBlockDR = building.BlockDR;
                    baseBlockUR = building.BlockUR;
                    baseFound = true;
                }
            }
        }

// 祭司探图定时:5分钟(7500 tick)~8分半(12750 tick)为强制探图窗口,
        // 不受敌袭标志(attackInComing≈3.7分钟就置真)阻断;8分半后停探回家。
        if (!allOut_Started && baseFound) {
            if (timer >= 7500 && timer < 12750) {
                needExploration = true;                 // 定时探图窗口内持续探图
            }
            if (timer >= 12750) {
                needExploration = false;                // 8分半后停探
                if (priestState == HUMAN_STATE_IDLE &&
                    abs(priestBlockDR - baseBlockDR) + abs(priestBlockUR - baseBlockUR) > 4) {
                    HumanMove(priestSN, (baseBlockDR - 1) * BLOCKSIDELENGTH,
                                         (baseBlockUR - 1) * BLOCKSIDELENGTH);   // 回家
                }
            }
        }

        bool exploreWindow = (timer >= 7500 && timer < 12750);   // 强制探图窗口(不受attackInComing阻断)
        if (needExploration && (!attackInComing || exploreWindow)) {
            // 找祭司(优先空闲的,别跟敌袭抢人)
            if (!priestFound) {
                for (const tagArmy& army : info.armies) {
                    if (army.Sort != AT_PRIEST) continue;
                    if (army.NowState != HUMAN_STATE_IDLE) continue;
                    priestSN = army.SN;
                    priestFound = true;
                }
            }

            if (priestFound && baseFound && info.theMap != nullptr) {
                // ----- 小工具 -----
                // 这块是不是已经探索(不是 Unknown)
                auto explore_isKnown = [&](int dr, int ur) -> bool {
                    if (dr < 0 || dr >= MAP_L || ur < 0 || ur >= MAP_U) return false;
                    return (*info.theMap)[dr][ur].type != MAPPATTERN_UNKNOWN;
                };
                // 这块的上下左右有没有贴着没探索的格子
                auto explore_touchUnknown = [&](int dr, int ur) -> bool {
                    const int dx[4] = { 1, -1, 0, 0 };
                    const int dy[4] = { 0, 0, 1, -1 };
                    for (int i = 0; i < 4; i++) {
                        int nd = dr + dx[i], nu = ur + dy[i];
                        if (nd < 0 || nd >= MAP_L || nu < 0 || nu >= MAP_U) continue;
                        if ((*info.theMap)[nd][nu].type == MAPPATTERN_UNKNOWN) return true;
                    }
                    return false;
                };
                // 建筑占地边长
                auto explore_buildSize = [](int type) -> int {
                    switch (type) {
                    case BUILDING_HOME: return 2;
                    case BUILDING_GRANARY: return 3;
                    case BUILDING_STOCK: return 3;
                    case BUILDING_FARM: return 3;
                    case BUILDING_ARROWTOWER: return 2;
                    case BUILDING_CENTER: return 3;
                    case BUILDING_ARMYCAMP: return 3;
                    case BUILDING_MARKET: return 3;
                    case BUILDING_STABLE: return 3;
                    case BUILDING_RANGE: return 3;
                    case BUILDING_DOCK: return 3;
                    case BUILDING_SIEGE: return 3;
                    case BUILDING_COLLAGE: return 3;
                    default: return 1;
                    }
                };

                // ----- 卡住检测:位置连续不变就强制换目标(防动态堵路) -----
                priestExplore_StillTimer++;
                if (priestExplore_StillTimer >= 25) {
                    priestExplore_StillTimer = 0;
                    if (priestExplore_PreDR == priestDR && priestExplore_PreUR == priestUR) {
                        priestExplore_StuckCnt++;
                        if (priestExplore_StuckCnt >= 3) {
                            priestExplore_StuckCnt = 0;
                            needNewTarget = true;
                        }
                    }
                    else {
                        priestExplore_StuckCnt = 0;
                    }
                    priestExplore_PreDR = priestDR;
                    priestExplore_PreUR = priestUR;
                }

                // 祭司站定了(到达/把活干完)=> 派新任务
                if (priestState == HUMAN_STATE_IDLE && coolDown == 0) {
                    needNewTarget = true;
                }

                if (needNewTarget && coolDown == 0) {
                    needNewTarget = false;

                    // ----- 建立障碍表:海洋 / 建筑占地 / 树 / 矿 -----
                    vector<vector<char>> blocked(MAP_L, vector<char>(MAP_U, 0));
                    for (int dr = 0; dr < MAP_L; dr++) {
                        for (int ur = 0; ur < MAP_U; ur++) {
                            if ((*info.theMap)[dr][ur].type == MAPPATTERN_OCEAN)
                                blocked[dr][ur] = 1;
                        }
                    }
                    for (const tagBuilding& b : info.buildings) {
                        int sz = explore_buildSize(b.Type);
                        for (int i = 0; i < sz; i++) {
                            for (int j = 0; j < sz; j++) {
                                if (b.BlockDR + i < MAP_L && b.BlockUR + j < MAP_U)
                                    blocked[b.BlockDR + i][b.BlockUR + j] = 1;
                            }
                        }
                    }
                    for (const tagResource& r : info.resources) {
                        if (r.Type == RESOURCE_TREE) {
                            blocked[r.BlockDR][r.BlockUR] = 1;
                        }
                        else if (r.Type == RESOURCE_STONE || r.Type == RESOURCE_GOLD) {
                            for (int i = 0; i < 2 && r.BlockDR + i < MAP_L; i++)
                                for (int j = 0; j < 2 && r.BlockUR + j < MAP_U; j++)
                                    blocked[r.BlockDR + i][r.BlockUR + j] = 1;
                        }
                    }
                    // 大本营自己必须是可走格(它脚下被建筑遮住了)
                    if (baseBlockDR >= 0 && baseBlockDR < MAP_L &&
                        baseBlockUR >= 0 && baseBlockUR < MAP_U)
                        blocked[baseBlockDR][baseBlockUR] = 0;

                    // ----- 从大本营 BFS 整个"已探索且能走到"的连通域 -----
                    vector<vector<int>> dist(MAP_L, vector<int>(MAP_U, -1));
                    queue<pair<int, int>> q;
                    dist[baseBlockDR][baseBlockUR] = 0;
                    q.push({ baseBlockDR, baseBlockUR });
                    const int dx[4] = { 1, -1, 0, 0 };
                    const int dy[4] = { 0, 0, 1, -1 };

                    int bestDist = 0x3f3f3f3f;
                    int bestDR = -1, bestUR = -1;
                    while (!q.empty()) {
                        int dr = q.front().first, ur = q.front().second;
                        q.pop();

                        if (explore_touchUnknown(dr, ur)) {
                            // 这是一个"推进入口":离基地越近越先探,
                            // 顺带避开祭司脚底下和上一个目标
                            int d = dist[dr][ur];
                            if (abs(dr - priestBlockDR) + abs(ur - priestBlockUR) <= 1) d += 100;
                            if (dr == targetBlockDR && ur == targetBlockUR) d += 50;
                            if (d < bestDist) {
                                bestDist = d;
                                bestDR = dr;
                                bestUR = ur;
                            }
                        }
                        for (int i = 0; i < 4; i++) {
                            int nd = dr + dx[i], nu = ur + dy[i];
                            if (nd < 0 || nd >= MAP_L || nu < 0 || nu >= MAP_U) continue;
                            if (blocked[nd][nu]) continue;          // 海/建筑/树/矿
                            if (dist[nd][nu] != -1) continue;
                            dist[nd][nu] = dist[dr][ur] + 1;
                            q.push({ nd, nu });
                        }
                    }

                    if (bestDR != -1) {
                        // 目标在已探索连通域内 => 寻路必然成功,不会卡死
                        targetBlockDR = bestDR;
                        targetBlockUR = bestUR;
                        HumanMove(priestSN, targetBlockDR * BLOCKSIDELENGTH,
                                          targetBlockUR * BLOCKSIDELENGTH);
                        coolDown = 40;   // 移动指令冷却,防止指令刷屏
                    }
                    else {
                        // 基地所在连通域已没有可推进的入口 => 周边探完,收工
                        needExploration = false;
                    }
                }
            }
        }

        // 探图结束回基地
        if (!needExploration && !explorationFinished) {
            explorationFinished = true;
            HumanMove(priestSN, baseBlockDR * BLOCKSIDELENGTH, baseBlockUR * BLOCKSIDELENGTH);
        }
        // 移动命令冷却
        if (coolDown > 0) {
            coolDown--;
        }
    }
    //--------------------发动总攻--------------------
    if(1){
        allOut_Tick++;
    if (!allOut_Started && timer >= ALLOUT_TRIGGER_TICK) {
        // 一次性标记:到 20 分钟(约30000 tick)就启动总攻
        allOut_Started = true;
        needExploration = false;          // 停掉前期探图
        needNewTarget = false;
        allOut_Targets.clear();
        DebugText("总攻开始:全员出门找营地");
    }

    if (allOut_Started && allOut_Tick % ALLOUT_CHECK_EVERY == 0) {
        // ----- 小工具 -----
        auto aO_isKnown = [&](int dr, int ur) -> bool {
            if (dr < 0 || dr >= MAP_L || ur < 0 || ur >= MAP_U) return false;
            return (*info.theMap)[dr][ur].type != MAPPATTERN_UNKNOWN;
        };
        auto aO_isOcean = [&](int dr, int ur) -> bool {
            if (dr < 0 || dr >= MAP_L || ur < 0 || ur >= MAP_U) return true;
            return (*info.theMap)[dr][ur].type == MAPPATTERN_OCEAN;
        };
        // 建筑占地边长
        auto aO_buildSize = [](int type) -> int {
            switch (type) {
            case BUILDING_HOME: return 2;
            case BUILDING_ARROWTOWER: return 2;
            case BUILDING_CENTER: return 3;
            case BUILDING_GRANARY: return 3;
            case BUILDING_STOCK: return 3;
            case BUILDING_FARM: return 3;
            case BUILDING_ARMYCAMP: return 3;
            case BUILDING_MARKET: return 3;
            case BUILDING_STABLE: return 3;
            case BUILDING_RANGE: return 3;
            case BUILDING_DOCK: return 3;
            case BUILDING_SIEGE: return 3;
            case BUILDING_COLLAGE: return 3;
            default: return 1;
            }
        };
        // 这块到底能不能站:海/树/石头/金矿/自家建筑/未知 全都不行
        auto aO_isBlocked = [&](int dr, int ur) -> bool {
            if (dr < 0 || dr >= MAP_L || ur < 0 || ur >= MAP_U) return true;
            if ((*info.theMap)[dr][ur].type == MAPPATTERN_UNKNOWN) return true;
            if ((*info.theMap)[dr][ur].type == MAPPATTERN_OCEAN) return true;
            for (const tagResource& r : info.resources) {
                if (r.Type == RESOURCE_TREE) {
                    if (dr == r.BlockDR && ur == r.BlockUR) return true;   // 树占 1 格
                }
                else if (r.Type == RESOURCE_STONE || r.Type == RESOURCE_GOLD) {
                    if (dr >= r.BlockDR && dr < r.BlockDR + 2 &&
                        ur >= r.BlockUR && ur < r.BlockUR + 2) return true; // 石/金占 2x2
                }
            }
            for (const tagBuilding& b : info.buildings) {
                int sz = aO_buildSize(b.Type);
                if (dr >= b.BlockDR && dr < b.BlockDR + sz &&
                    ur >= b.BlockUR && ur < b.BlockUR + sz) return true;
            }
            return false;
        };
        // 某个格子四邻有没有没探索的雾(说明这里是已探索区域的边界)
        auto aO_nearUnknown = [&](int dr, int ur) -> bool {
            const int dx[4] = { 1, -1, 0, 0 };
            const int dy[4] = { 0, 0, 1, -1 };
            for (int i = 0; i < 4; i++) {
                int nd = dr + dx[i], nu = ur + dy[i];
                if (nd < 0 || nd >= MAP_L || nu < 0 || nu >= MAP_U) continue;
                if ((*info.theMap)[nd][nu].type == MAPPATTERN_UNKNOWN) return true;
            }
            return false;
        };
        // 营地资格:离家 12~42 块、离地图边至少 8 块、周围 14 格无敌人、
        //           12 格半径内已探索陆地>=60%(不允许把营地定在地图边角)
        auto aO_campOK = [&](int dr, int ur) -> bool {
            if (!aO_isKnown(dr, ur)) return false;
            int manBase = abs(dr - baseBlockDR) + abs(ur - baseBlockUR);
            if (manBase < 12 || manBase > 42) return false;            // 离基地太近/太远都不行
            if (dr < 8 || dr >= MAP_L - 8 || ur < 8 || ur >= MAP_U - 8) return false; // 贴边不行
            for (const tagArmy& e : info.enemy_armies) {
                if (abs(e.BlockDR - dr) + abs(e.BlockUR - ur) < 14) return false;
            }
            int total = 0, land = 0;
            for (int i = -12; i <= 12; i++) {
                for (int j = -12; j <= 12; j++) {
                    int nd = dr + i, nu = ur + j;
                    if (nd < 0 || nd >= MAP_L || nu < 0 || nu >= MAP_U) continue;
                    total++;
                    if (aO_isKnown(nd, nu) && !aO_isOcean(nd, nu)) land++;
                }
            }
            return total > 0 && land * 10 >= total * 6;
        };
        // 从基地 BFS 整个"已探索且能走到"的连通域,
        // 给每个方向挑一个"离基地较近(20~45)、真正可达"的贴雾边界格,
        // 兵像水波一样逐层推进;近区探完才放宽到更远(45~75)兜底;
        // 目标保证可达:不会卡树林/死角,也不会一下跳去地图边/湖里。
        const int aO_dR[4] = { 1, 0, -1, 0 };
        const int aO_dU[4] = { 0, 1, 0, -1 };
        auto aO_bfsFrontiers = [&]() -> array<pair<int, int>, 4> {
            array<pair<int, int>, 4> resNear = { { { -1, -1 }, { -1, -1 }, { -1, -1 }, { -1, -1 } } };
            array<pair<int, int>, 4> resFar = { { { -1, -1 }, { -1, -1 }, { -1, -1 }, { -1, -1 } } };
            array<pair<int, int>, 4> res = { { { -1, -1 }, { -1, -1 }, { -1, -1 }, { -1, -1 } } };
            if (baseBlockDR < 0 || baseBlockDR >= MAP_L || baseBlockUR < 0 || baseBlockUR >= MAP_U) return res;
            vector<vector<int>> dist(MAP_L, vector<int>(MAP_U, -1));
            queue<pair<int, int>> q;
            dist[baseBlockDR][baseBlockUR] = 0;   // 起点=大本营(脚下即使被建筑占也照走)
            q.push({ baseBlockDR, baseBlockUR });
            array<int, 4> bestNear = { -1, -1, -1, -1 };
            array<int, 4> bestFar = { -1, -1, -1, -1 };
            while (!q.empty()) {
                int dr = q.front().first, ur = q.front().second;
                q.pop();
                if (aO_nearUnknown(dr, ur)) {          // 贴雾 = 边界格
                    // 湖畔/海角不选:邻格已知海>=2 的边界推不动,兵会在湖边死磕
                    int seaCnt = 0;
                    for (int d = 0; d < 4; d++) {
                        int nd = dr + aO_dR[d], nu = ur + aO_dU[d];
                        if (nd < 0 || nd >= MAP_L || nu < 0 || nu >= MAP_U) continue;
                        if (aO_isOcean(nd, nu)) seaCnt++;
                    }
                    if (seaCnt >= 1) continue;   // 贴水边(邻海>=1)的边界也不选:兵至少离水1格
                    for (int d = 0; d < 4; d++) {
                        int proj = aO_dR[d] * (dr - baseBlockDR) + aO_dU[d] * (ur - baseBlockUR);
                        if (proj >= 20 && proj <= 45) {
                            if (proj > bestNear[d]) { bestNear[d] = proj; resNear[d] = { dr, ur }; }
                        }
                        else if (proj > 45 && proj <= 75) {
                            if (proj > bestFar[d]) { bestFar[d] = proj; resFar[d] = { dr, ur }; }
                        }
                    }
                }
                for (int d = 0; d < 4; d++) {
                    int nd = dr + aO_dR[d], nu = ur + aO_dU[d];
                    if (nd < 0 || nd >= MAP_L || nu < 0 || nu >= MAP_U) continue;
                    if (dist[nd][nu] != -1) continue;
                    if (aO_isBlocked(nd, nu)) continue;   // 树/海/建筑/矿/未知都不进 BFS
                    // 贴水边的格子不通行,保证兵至少离池塘/海 1 格
                    int sea2 = 0;
                    for (int k = 0; k < 4; k++) {
                        int snd = nd + aO_dR[k], snu = nu + aO_dU[k];
                        if (snd < 0 || snd >= MAP_L || snu < 0 || snu >= MAP_U) continue;
                        if (aO_isOcean(snd, snu)) sea2++;
                    }
                    if (sea2 >= 1) continue;
                    dist[nd][nu] = dist[dr][ur] + 1;
                    q.push({ nd, nu });
                }
            }
            for (int d = 0; d < 4; d++) {
                if (resNear[d].first != -1) res[d] = resNear[d];   // 近区优先
                else res[d] = resFar[d];                           // 近区没货才用远区兜底
            }
            return res;
        };
        // 读某个兵当前方向;没记过就按它编号分方向
        auto aO_getDir = [&](int sn) -> int {
            auto it = allOut_Dir.find(sn);
            return (it == allOut_Dir.end()) ? ((sn % 4 + 4) % 4) : it->second;
        };

        if (!allOut_CampFounded) {
            // ----- 阶段1:先找到敌人的营地/敌人,发现之前绝不集合 -----
            // 1) 只有看到敌方"建筑"才算找到敌方基地;野外零星小兵/农民不能定位营地
            if (!allOut_EnemySpotted) {
                int spotDR = -1, spotUR = -1, bestD = 0x3f3f3f3f;
                for (const tagBuilding& eb : info.enemy_buildings) {
                    int d = abs(eb.BlockDR - baseBlockDR) + abs(eb.BlockUR - baseBlockUR);
                    if (d < bestD) { bestD = d; spotDR = eb.BlockDR; spotUR = eb.BlockUR; }
                }
                // 不再用 farmer/army 定位营地:野外零星小杂兵看到就集合会出错
                if (spotDR != -1) {
                    allOut_EnemySpotted = true;
                    allOut_EnemySpotDR = spotDR;
                    allOut_EnemySpotUR = spotUR;
                    // 营地中心 = 敌方基地(最近敌建筑)朝我方这边偏 12 块(站外围,不贴脸)
                    int dirR = (spotDR - baseBlockDR >= 0) ? 1 : -1;
                    int dirU = (spotUR - baseBlockUR >= 0) ? 1 : -1;
                    allOut_CampDR = spotDR - dirR * 12;
                    allOut_CampUR = spotUR - dirU * 12;
                    allOut_CampFounded = true;
                    allOut_Targets.clear();
                    DebugText("发现敌方基地(敌建筑),全员向敌营集合");
                }
                // 只有零散敌人(农民/兵)而没有建筑 => 不集合,继续推进找真正的基地
            }
        }
        // 2) 还没发现敌人:兵分多路,沿各自方向一直往未探明处大步推进(目标必可达)
            if (!allOut_CampFounded) {
                array<pair<int, int>, 4> frts = aO_bfsFrontiers();   // 一次 BFS,四方向各一个可达边界
                for (tagArmy& a : info.armies) {
                    if (a.Sort == AT_PRIEST) continue;          // 祭司不参与战斗
                    if (a.NowState != HUMAN_STATE_IDLE) continue;
                    int dir = aO_getDir(a.SN);                  // 读该兵方向,默认按编号分向
                    // 当前方向探到头就立即换下一个方向,不傻等一拍
                    for (int k = 0; k < 4; k++) {
                        int nd = (dir + k) % 4;
                        const pair<int, int>& tg = frts[nd];
                        if (tg.first != -1) {
                            allOut_Dir[a.SN] = nd;              // 记住新方向
                            HumanMove(a.SN, tg.first * BLOCKSIDELENGTH,
                                             tg.second * BLOCKSIDELENGTH);
                            break;
                        }
                    }
                    // 四个方向都推不动(水/地图边缘四面包围)才原地等下一轮
                }
                // ----- 祭司也往同一个方向跟(不用等它、不参与战斗) -----
                if (priestState == HUMAN_STATE_IDLE) {
                    const pair<int, int>& priestTg = frts[(priestSN % 4 + 4) % 4];
                    if (priestTg.first != -1) {
                        HumanMove(priestSN, priestTg.first * BLOCKSIDELENGTH,
                                         priestTg.second * BLOCKSIDELENGTH);
                    }
                }
            }
        else {
            // ----- 阶段2:敌营已定位 —— 先集合,集合完毕才开始攻打 -----
            // 人口达标(≥50,一次性)也视为可总攻,不被"集合85%"卡住
            if (!allOut_AttackEnabled)
                allOut_AttackEnabled = (info.armies.size() + info.farmers.size() >= 50);
            // 集合完毕(70%到位,一次性锁定)后才开始攻打;新兵加入不会让已锁定的"集合完毕"回退。
            if (!allOut_Gathered) {
                int totCnt = 0, nearCnt = 0;
                for (const tagArmy& a : info.armies) {
                    if (a.Sort == AT_PRIEST) continue;
                    int d = abs(a.BlockDR - allOut_CampDR) + abs(a.BlockUR - allOut_CampUR);
                    if (d > 25) continue;   // 极远处的(新兵/还在家)不计入,免得总是集不齐
                    totCnt++;
                    if (d <= 14) nearCnt++;
                }
                bool gatheredNow = (totCnt == 0) || (nearCnt * 100 >= totCnt * 85);  // 已靠近战场的兵85%到位=集合完毕(可调)
                if (gatheredNow) allOut_Gathered = true;
            }

            if (allOut_Gathered || allOut_AttackEnabled) {
                // -------------------- 2B:集合完毕/人口达标 --------------------
                if (!allOut_AttackEnabled) {
                    // 消耗阶段:总人口未到50,兵只围观站位,祭司 Hit-and-Run 招降耗敌
                    if (!allOut_AttackEnabled) allOut_AttackEnabled = (info.armies.size() + info.farmers.size() >= 50);   // 一次性:达标后永不回退
                    if (!allOut_AttackEnabled) {
                        // 兵:只站外围看,不动手
                        for (tagArmy& a : info.armies) {
                            if (a.Sort == AT_PRIEST) continue;
                            if (a.NowState != HUMAN_STATE_IDLE) continue;
                            int slot = (int)(a.SN % 12), offR = (slot % 4) - 1, offU = (slot / 4) - 1;
                            if (offR == 0 && offU == 0) offR = 3;
                            int tgDR = allOut_CampDR + offR * 4, tgUR = allOut_CampUR + offU * 4;
                            if (aO_isBlocked(tgDR, tgUR)) { tgDR = allOut_CampDR; tgUR = allOut_CampUR + 8; }
                            HumanMove(a.SN, tgDR * BLOCKSIDELENGTH, tgUR * BLOCKSIDELENGTH);
                        }
                        // 祭司:先判断是否被敌人锁定(近身威胁<5格),被锁就放弃一切立刻逃
                        double threatD = 1e18; int threatR = -1, threatU = -1;
                        for (const tagArmy& e : info.enemy_armies) {
                            double dd = calDistance(priestDR, priestUR, e.DR, e.UR);
                            if (dd < threatD) { threatD = dd; threatR = e.BlockDR; threatU = e.BlockUR; }
                        }
                        for (const tagFarmer& f : info.enemy_farmers) {
                            double dd = calDistance(priestDR, priestUR, f.DR, f.UR);
                            if (dd < threatD) { threatD = dd; threatR = f.BlockDR; threatU = f.BlockUR; }
                        }
                        bool lockedThreat = (threatR != -1 && threatD < 5.0 * BLOCKSIDELENGTH);
                        if (lockedThreat) {
                            // 被锁定:立即打断一切(含招降),朝敌人反方向逃 6 格
                            int rr = (priestBlockDR > threatR) ? 1 : ((priestBlockDR < threatR) ? -1 : 0);
                            int uu = (priestBlockUR > threatU) ? 1 : ((priestBlockUR < threatU) ? -1 : 0);
                            int mdr = priestBlockDR + rr * 4, mur = priestBlockUR + uu * 4;
                            if (aO_isBlocked(mdr, mur)) { mdr = priestBlockDR + rr * 3; mur = priestBlockUR + uu * 3; }
                            if (!aO_isBlocked(mdr, mur)) {
                                HumanMove(priestSN, mdr * BLOCKSIDELENGTH, mur * BLOCKSIDELENGTH);
                            }
                        }
                        else if (priestState == HUMAN_STATE_IDLE) {
                            // 没被锁:招降或冷却
                            int cSN = -1; double cD = 1e18; int cDR = -1, cUR = -1;
                            for (const tagArmy& e : info.enemy_armies) {
                                double dd = calDistance(priestDR, priestUR, e.DR, e.UR);
                                if (dd < cD) { cD = dd; cSN = e.SN; cDR = e.BlockDR; cUR = e.BlockUR; }
                            }
                            for (const tagFarmer& f : info.enemy_farmers) {
                                double dd = calDistance(priestDR, priestUR, f.DR, f.UR);
                                if (dd < cD) { cD = dd; cSN = f.SN; cDR = f.BlockDR; cUR = f.BlockUR; }
                            }
                            if (canConvert && cSN != -1) {
                                if (cD <= DIS_PRIEST * BLOCKSIDELENGTH) {
                                    HumanAction(priestSN, cSN);      // 招降最近
                                }
                                else if (cD <= 14.0 * BLOCKSIDELENGTH) {
                                    HumanMove(priestSN, cDR * BLOCKSIDELENGTH, cUR * BLOCKSIDELENGTH);   // 上去招
                                }
                            }
                            else {
                                // 招完/冷却/有目标,又没被锁:别跑太远,只小退 4 格,省得冷却好还要赶路
                                if (cSN != -1 && cD < 9.0 * BLOCKSIDELENGTH) {
                                    int rr = (priestBlockDR > cDR) ? 1 : ((priestBlockDR < cDR) ? -1 : 0);
                                    int uu = (priestBlockUR > cUR) ? 1 : ((priestBlockUR < cUR) ? -1 : 0);
                                    int mdr = priestBlockDR + rr * 4, mur = priestBlockUR + uu * 4;
                                    if (!aO_isBlocked(mdr, mur)) {
                                        HumanMove(priestSN, mdr * BLOCKSIDELENGTH, mur * BLOCKSIDELENGTH);
                                    }
                                }
                                // 视野没人/已安全:原地等(绝不远跑),冷却好直接再上前招
                            }
                        }
                    }
                }
                if (allOut_AttackEnabled) {
                    // -------------------- 正式总攻(总人口达标):最简——空闲兵打最近目标 --------------------
                    bool anyEnemy = !info.enemy_armies.empty() || !info.enemy_farmers.empty()
                                 || !info.enemy_buildings.empty();
                    // ---- 只剩兵工厂:全军停手,让祭司贴邻转化 ----
                    bool onlySiegeLeft = info.enemy_armies.empty() && info.enemy_farmers.empty()
                                      && !info.enemy_buildings.empty();
                    if (onlySiegeLeft) {
                        for (const tagBuilding& eb : info.enemy_buildings) {
                            if (eb.Type != BUILDING_SIEGE) { onlySiegeLeft = false; break; }
                        }
                    }
                    if (onlySiegeLeft) {
                        // 兵不打:回外围站位
                        for (tagArmy& a : info.armies) {
                            if (a.Sort == AT_PRIEST) continue;
                            if (a.NowState != HUMAN_STATE_IDLE) continue;
                            int slot = (int)(a.SN % 12), offR = (slot % 4) - 1, offU = (slot / 4) - 1;
                            if (offR == 0 && offU == 0) offR = 3;
                            int tgDR = allOut_CampDR + offR * 4, tgUR = allOut_CampUR + offU * 4;
                            if (aO_isBlocked(tgDR, tgUR)) { tgDR = allOut_CampDR; tgUR = allOut_CampUR + 8; }
                            HumanMove(a.SN, tgDR * BLOCKSIDELENGTH, tgUR * BLOCKSIDELENGTH);
                        }
                        // 祭司:去贴邻兵工厂转化
                        if (priestState == HUMAN_STATE_IDLE) {
                            int sSN = -1, sDR = -1, sUR = -1;
                            for (const tagBuilding& eb : info.enemy_buildings) {
                                if (eb.Type == BUILDING_SIEGE) { sSN = eb.SN; sDR = eb.BlockDR; sUR = eb.BlockUR; break; }
                            }
                            if (sSN != -1) {
                                int dd = abs(priestBlockDR - sDR) + abs(priestBlockUR - sUR);
                                if (dd <= 5) HumanAction(priestSN, sSN);
                                else HumanMove(priestSN, sDR * BLOCKSIDELENGTH, sUR * BLOCKSIDELENGTH);
                            }
                        }
                    }
                    else if (anyEnemy) {
                        for (tagArmy& a : info.armies) {
                            if (a.Sort == AT_PRIEST) continue;
                            if (a.NowState != HUMAN_STATE_IDLE) continue;
                            long long bst = 0x7fffffffffffffffLL; int t = -1;
                            for (const tagArmy& e : info.enemy_armies) {
                                long long sc = (long long)abs(e.BlockDR - a.BlockDR) + abs(e.BlockUR - a.BlockUR);
                                if (sc < bst) { bst = sc; t = e.SN; }
                            }
                            for (const tagFarmer& f : info.enemy_farmers) {
                                long long sc = (long long)abs(f.BlockDR - a.BlockDR) + abs(f.BlockUR - a.BlockUR);
                                if (sc < bst && t == -1) { if (sc < bst) { bst = sc; t = f.SN; } }
                            }
                            if (t == -1) {
                                for (const tagBuilding& b2 : info.enemy_buildings) {
                                    if (b2.Type == BUILDING_SIEGE) continue;   // 兵工厂留给祭司转化,军队不打
                                    long long sc = (long long)abs(b2.BlockDR - a.BlockDR) + abs(b2.BlockUR - a.BlockUR);
                                    if (sc < bst) { bst = sc; t = b2.SN; }
                                }
                            }
                            if (t != -1) {
                                HumanAction(a.SN, t);
                            }
                        }
                    }
                    else {
                        // 无敌:兵回外围站位
                        for (tagArmy& a : info.armies) {
                            if (a.Sort == AT_PRIEST) continue;
                            if (a.NowState != HUMAN_STATE_IDLE) continue;
                            int slot = (int)(a.SN % 12), offR = (slot % 4) - 1, offU = (slot / 4) - 1;
                            if (offR == 0 && offU == 0) offR = 3;
                            int tgDR = allOut_CampDR + offR * 4, tgUR = allOut_CampUR + offU * 4;
                            if (aO_isBlocked(tgDR, tgUR)) {
                                tgDR = allOut_CampDR + (offR == 0 ? 4 : -offR) * 3;
                                tgUR = allOut_CampUR + (offU == 0 ? 4 : -offU) * 3;
                            }
                            HumanMove(a.SN, tgDR * BLOCKSIDELENGTH, tgUR * BLOCKSIDELENGTH);
                        }
                    }
                }
            else {
                // ---- 2A 集合阶段:全军向敌营外围靠拢,先不打 ----
                for (tagArmy& a : info.armies) {
                    if (a.Sort == AT_PRIEST) continue;
                    if (a.NowState != HUMAN_STATE_IDLE) continue;
                    int slot = (int)(a.SN % 12);
                    int offR = (slot % 4) - 1;                   // -1~2
                    int offU = (slot / 4) - 1;                   // -1~1
                    if (offR == 0 && offU == 0) offR = 3;        // 别站在营地正中心
                    int tgDR = allOut_CampDR + offR * 4;         // 每人隔 4 块
                    int tgUR = allOut_CampUR + offU * 4;
                    if (aO_isBlocked(tgDR, tgUR)) {              // 落点卡(海/树/建筑)就换个偏位
                        tgDR = allOut_CampDR;
                        tgUR = allOut_CampUR + 8;
                    }
                    HumanMove(a.SN, tgDR * BLOCKSIDELENGTH,
                                     tgUR * BLOCKSIDELENGTH);
                }
            }

            // ----- 阶段3:祭司也去,但站远一点;敌人(含建筑)被清空后它才动手 -----
            bool enemyHasArrowTower = false;
            for (const tagBuilding& eb : info.enemy_buildings) {
                if (eb.Type == BUILDING_ARROWTOWER) { enemyHasArrowTower = true; break; }
            }
            allOut_EnemyCleared = info.enemy_armies.empty() && info.enemy_farmers.empty()
                               && !enemyHasArrowTower;   // 敌方箭塔没拆完,祭司不进场
            if (priestState != HUMAN_STATE_IDLE) {
                // 正在走路/转化,交给引擎走完
            }
            else if (!allOut_EnemyCleared) {
                // 战斗没打完:祭司远处观望(绝不动手),并且绝不进敌方箭塔射程
                auto priestWatchSafe = [&](int dr, int ur) -> bool {
                    if (aO_isBlocked(dr, ur)) return false;
                    if (dr < 2 || dr >= MAP_L - 2 || ur < 2 || ur >= MAP_U - 2) return false;
                    for (const tagBuilding& eb : info.enemy_buildings) {
                        if (eb.Type != BUILDING_ARROWTOWER) continue;
                        if (abs(eb.BlockDR - dr) + abs(eb.BlockUR - ur) <= DIS_ARROWTOWER + 1) return false;   // 在箭塔射程内,不能去
                    }
                    return true;
                };
                int bestDR = -1, bestUR = -1, bestSafe = -1;
                const int watchCand[6][2] = { { -12, -3 }, { 3, -12 }, { 12, 3 }, { -3, 12 }, { -10, 10 }, { 10, -10 } };
                for (int i = 0; i < 6; i++) {
                    int cd = allOut_CampDR + watchCand[i][0];
                    int cu = allOut_CampUR + watchCand[i][1];
                    if (!priestWatchSafe(cd, cu)) continue;
                    int minDist = 9999;
                    for (const tagBuilding& eb : info.enemy_buildings) {
                        if (eb.Type != BUILDING_ARROWTOWER) continue;
                        int dd = abs(eb.BlockDR - cd) + abs(eb.BlockUR - cu);
                        if (dd < minDist) minDist = dd;
                    }
                    if (minDist > bestSafe) { bestSafe = minDist; bestDR = cd; bestUR = cu; }
                }
                if (bestDR != -1) {
                    HumanMove(priestSN, bestDR * BLOCKSIDELENGTH, bestUR * BLOCKSIDELENGTH);
                }
            }
            else {
                // ----- 阶段4:敌方单位清空,祭司进场:优先转化敌方武器工程厂,其次其他敌建筑 -----
                int tSN = -1, tDR = -1, tUR = -1;
                double tBest = 1e18;
                // 1) 敌方单位残余(兜底;正常已空)
                for (const tagArmy& e : info.enemy_armies) {
                    double dd = calDistance(priestDR, priestUR, e.DR, e.UR);
                    if (dd < tBest) { tBest = dd; tSN = e.SN; tDR = e.BlockDR; tUR = e.BlockUR; }
                }
                for (const tagFarmer& e : info.enemy_farmers) {
                    double dd = calDistance(priestDR, priestUR, e.DR, e.UR);
                    if (dd < tBest) { tBest = dd; tSN = e.SN; tDR = e.BlockDR; tUR = e.BlockUR; }
                }
                // 2) 没有单位可转 → 优先敌方武器工程厂(BUILDING_SIEGE),其次其他已建成敌建筑
                if (tSN == -1) {
                    int fallbackSN = -1, fbDR = -1, fbUR = -1;
                    for (const tagBuilding& eb : info.enemy_buildings) {
                        if (eb.Percent < 100) continue;          // 未建成(地基)不能转
                        if (eb.Type == BUILDING_SIEGE) {         // 兵工厂优先
                            tSN = eb.SN; tDR = eb.BlockDR; tUR = eb.BlockUR;
                            tBest = 0;
                            break;
                        }
                        if (fallbackSN == -1) { fallbackSN = eb.SN; fbDR = eb.BlockDR; fbUR = eb.BlockUR; }
                    }
                    if (tSN == -1) { tSN = fallbackSN; tDR = fbDR; tUR = fbUR; }
                }
                // 3) 够近就贴邻转化,还远就先靠近
                if (tSN != -1 && tDR != -1) {
                    int dBlocks = abs(priestBlockDR - tDR) + abs(priestBlockUR - tUR);
                    if (dBlocks <= 5) {
                        HumanAction(priestSN, tSN);              // 贴邻转化(引擎会再校准距离)
                    }
                    else {
                        HumanMove(priestSN, tDR * BLOCKSIDELENGTH,
                                         tUR * BLOCKSIDELENGTH);
                    }
                }
                else {
                    // 没有可转化的:祭司去营地中心守着
                    HumanMove(priestSN, allOut_CampDR * BLOCKSIDELENGTH,
                                     allOut_CampUR * BLOCKSIDELENGTH);
                }
            }
        }
    }
    }
    //--------------------农民工作--------------------
    if (1) {
    //自刎归天!!!
    if(timer==33000){
        int suicided=0;
        for(tagHuman &farmer:info.farmers){
            HumanAction(farmer.SN,farmer.SN);
            suicided++;
            if(suicided==12)break;
        }
    }
    //分配工作,暂定 浆果:木头:建造:打猎:农田:石头=2:(3+6):2:6:4:1
    auto classify = [&](int farmerSN) {
        static int mark = 0;
        if (mark % 24 <= 1)berryFarmers.push_back(farmerSN);
        else if (mark % 24 <= 4)woodFarmers.push_back(farmerSN);
        else if (mark % 24 <= 6)buildingFarmers.push_back(farmerSN);
        else if (mark % 24 <= 12)hunterFarmers.push_back(farmerSN);
        else if (mark % 24 <= 18)woodFarmers.push_back(farmerSN);
        else if (mark % 24 <= 22)farmFarmers.push_back(farmerSN);
        else stoneFarmers.push_back(farmerSN);
        DebugText("农民人口=");
        DebugText(mark + 1);
        DebugText("采果子人数=");
        DebugText((int)berryFarmers.size());
        DebugText("砍树人数=");
        DebugText((int)woodFarmers.size());
        DebugText("建造人数=");
        DebugText((int)buildingFarmers.size());
        DebugText("打猎人数=");
        DebugText((int)hunterFarmers.size());
        DebugText("农田人数=");
        DebugText((int)farmFarmers.size());
        DebugText("采石头人数=");
        DebugText((int)stoneFarmers.size());
        mark++;
        };

    for (tagFarmer& farmer : info.farmers) {
        int SN = farmer.SN;
        bool classified = false;
        for (int id : berryFarmers) {
            if (id == SN) {
                classified = true;
                break;
            }
        }
        if (!classified) {
            for (int id : woodFarmers) {
                if (id == SN) {
                    classified = true;
                    break;
                }
            }
        }
        if (!classified) {
            for (int id : stoneFarmers) {
                if (id == SN) {
                    classified = true;
                    break;
                }
            }
        }
        if (!classified) {
            for (int id : buildingFarmers) {
                if (id == SN) {
                    classified = true;
                    break;
                }
            }
        }
        if (!classified) {
            for (int id : hunterFarmers) {
                if (id == SN) {
                    classified = true;
                    break;
                }
            }
        }
        if (!classified) {
            for (int id : farmFarmers) {
                if (id == SN) {
                    classified = true;
                    break;
                }
            }
        }
        if (!classified) {
            classify(SN);
            DebugText(SN);
        }
    }

    auto assignTask = [&](vector<int>farmers, int type) {
        for (int SN : farmers) {
            for (tagFarmer& farmer : info.farmers) {
                if (farmer.SN != SN)continue;
                if (farmer.NowState != HUMAN_STATE_IDLE)break;
                int bestResourceSN = -1;
                double minDist = 1e18;
                double dist = 1e18;
                for (tagResource& resource : info.resources) {
                    if (resource.Type != type)continue;
                    dist = calDistance(farmer.DR, farmer.UR, resource.DR, resource.UR);
                    if (dist < minDist) {
                        minDist = dist;
                        bestResourceSN = resource.SN;
                    }
                }
                if (bestResourceSN != -1) {
                    HumanAction(farmer.SN, bestResourceSN);
                }
                break;
            }
        }
        };
    if(timer>18000){
        if(info.Meat*7<info.Wood*4){
            assignTask(buildingFarmers,RESOURCE_ELEPHANT);
            assignTask(buildingFarmers,RESOURCE_GAZELLE);
            assignTask(berryFarmers,RESOURCE_ELEPHANT);
            assignTask(berryFarmers,RESOURCE_GAZELLE);
            assignTask(stoneFarmers,RESOURCE_ELEPHANT);
            assignTask(stoneFarmers,RESOURCE_GAZELLE);
            assignTask(woodFarmers,RESOURCE_ELEPHANT);
            assignTask(woodFarmers,RESOURCE_GAZELLE);
        }else{
            assignTask(buildingFarmers,RESOURCE_TREE);
            assignTask(berryFarmers,RESOURCE_TREE);
            assignTask(stoneFarmers,RESOURCE_TREE);
            assignTask(woodFarmers,RESOURCE_TREE);
        }
    }
    if(timer<21750)assignTask(berryFarmers, RESOURCE_BUSH);
    if(timer<20000)assignTask(woodFarmers, RESOURCE_TREE);
    if(timer<18000)assignTask(stoneFarmers, RESOURCE_GOLD);
    if(timer<22500){
        for (tagBuilding& building : info.buildings) {
            if (building.Type != BUILDING_FARM)continue;
            for (int SN : farmFarmers) {
                for (tagFarmer& farmer : info.farmers) {
                    if (farmer.SN != SN)continue;
                    if (farmer.NowState != HUMAN_STATE_IDLE)break;
                    HumanAction(SN, building.SN);
                }
            }
        }
    }
    bool lionFound = false;
    for (tagResource& resource : info.resources) {
        if (resource.Type == RESOURCE_ELEPHANT) {
            lionFound = true;
            break;
        }
    }
    bool elephantFound = false;
    for (tagResource& resource : info.resources) {
        if (resource.Type == RESOURCE_ELEPHANT) {
            elephantFound = true;
            break;
        }
    }
    //击杀优先级 象>瞪羚
    if (hunterFarmers.size() >= 5 && elephantFound) {
        assignTask(hunterFarmers, RESOURCE_ELEPHANT);
    }
    else {
        assignTask(hunterFarmers, RESOURCE_GAZELLE);
    }
    //盖建筑
    if(1){
        //记录建筑个数
        int homeCnt = 0, granaryCnt = 0, stockCnt = 0;
        int farmCnt = 0, arrowtowerCnt = 0, armycampCnt = 0;
        int stableCnt = 0, rangeCnt = 0, siegeCnt = 0;
        int marketCnt = 0, collageCnt=0;
        int homeCnt1 = 0, granaryCnt1 = 0, stockCnt1 = 0;
        int farmCnt1 = 0, arrowtowerCnt1 = 0, armycampCnt1 = 0;
        int stableCnt1 = 0, rangeCnt1 = 0, siegeCnt1 = 0;
        int marketCnt1 = 0, collageCnt1=0;
        for (tagBuilding& building : info.buildings) {
            if (building.Percent != 100)continue;
            switch (building.Type) {
            case BUILDING_HOME:homeCnt++; break;
            case BUILDING_GRANARY:granaryCnt++; break;
            case BUILDING_STOCK:stockCnt++; break;
            case BUILDING_FARM:farmCnt++; break;
            case BUILDING_ARROWTOWER:arrowtowerCnt++; break;
            case BUILDING_ARMYCAMP:armycampCnt++; break;
            case BUILDING_STABLE:stableCnt++; break;
            case BUILDING_RANGE:rangeCnt++; break;
            case BUILDING_SIEGE:siegeCnt++; break;
            case BUILDING_MARKET:marketCnt++; break;
            case BUILDING_COLLAGE:collageCnt++; break;
            default:break;
            }
        }
        for (tagBuilding& building : info.buildings) {
            switch (building.Type) {
            case BUILDING_HOME:homeCnt1++; break;
            case BUILDING_GRANARY:granaryCnt1++; break;
            case BUILDING_STOCK:stockCnt1++; break;
            case BUILDING_FARM:farmCnt1++; break;
            case BUILDING_ARROWTOWER:arrowtowerCnt1++; break;
            case BUILDING_ARMYCAMP:armycampCnt1++; break;
            case BUILDING_STABLE:stableCnt1++; break;
            case BUILDING_RANGE:rangeCnt1++; break;
            case BUILDING_SIEGE:siegeCnt1++; break;
            case BUILDING_MARKET:marketCnt1++; break;
            case BUILDING_COLLAGE:collageCnt1++; break;
            default:break;
            }
        }
        auto findOptimumPos = [&](int type) {
            int size = getBuildingSize(type) + 1;
            int bestDR = -1;
            int bestUR = -1;
            double bestScore = -1e9;
            for (int dr = 1; dr < MAP_L - size - 1; dr++) {
                for (int ur = 1; ur < MAP_U - size - 1; ur++) {
                    bool canBuild = true;
                    for (int i = 0; i < size && canBuild; i++) {
                        for (int j = 0; j < size && canBuild; j++) {
                            if (curMap[dr + i][ur + j] != 0) {
                                canBuild = false;
                                break;
                            }
                        }
                    }
                    if (!canBuild)continue;
                    double score = 0;
                    

                    if (type == BUILDING_ARROWTOWER) {
                        for (tagBuilding& building : info.buildings) {
                            if (building.Type != type)continue;
                            int dToSameType = abs(dr - building.BlockDR) + abs(ur - building.BlockUR);
                            score -= dToSameType;
                        }
                        if (abs(dr - baseBlockDR) <= 4 || abs(ur - baseBlockUR) <= 4)score -= 10;
                        if (abs(dr - baseBlockDR) >= 6 || abs(ur - baseBlockUR) >= 6)score -= 10;
                    }
                    else if (type == BUILDING_FARM) {
                        for (tagBuilding& building : info.buildings) {
                            if (building.Type != BUILDING_GRANARY)continue;
                            /*if (abs(dr - building.BlockDR) <= 3 || abs(ur - building.BlockDR) <= 3)score -= 10;
                            if (abs(dr - building.BlockDR) >= 6 || abs(ur - building.BlockDR) >= 6)score -= 10;
                            if (abs(dr - building.BlockDR) >= 10 || abs(ur - building.BlockDR) >= 10)score -= 100;*/
                            int dToGranary= abs(dr - building.BlockDR) + abs(ur - building.BlockUR);
                            score += (10 - dToGranary);
                        }
                    }
                    else {
                        int dToBase = abs(dr - baseBlockDR)+abs(ur - baseBlockUR);
                        score += (10 - dToBase);
                        /*if (abs(dr - baseBlockDR) + abs(ur - baseBlockUR) <= 3)score -= 30;
                        if (abs(dr - baseBlockDR) + abs(ur - baseBlockUR) >= 6)score -= 10;
                        if (abs(dr - baseBlockDR) + abs(ur - baseBlockUR) >= 20)score -= 50;
                        if (abs(dr - baseBlockDR) == 3 || abs(ur - baseBlockUR) == 3)score -= 50;*/
                        for (tagBuilding& building : info.buildings) {
                            if (building.Type != type)continue;
                            int dToSameType = abs(dr - building.BlockDR) + abs(ur - building.BlockUR);
                            score += (10 - dToSameType) * 2;
                        }
                    }
                    if (score > bestScore) {
                        bestScore = score;
                        bestDR = dr;
                        bestUR = ur;
                    }
                }
            }
            return pair<int, int>{bestDR, bestUR};
            };
        auto build = [&](int type) {
            vector<int>*farmers=&buildingFarmers;
            if (type == BUILDING_FARM) {
                farmers = &farmFarmers;
            }
            for (int SN : *farmers) {
                for (tagFarmer& farmer : info.farmers) {
                    if (farmer.SN != SN)continue;
                    if (farmer.NowState != HUMAN_STATE_IDLE)break;
                    pair<int, int>pos = findOptimumPos(type);
                    if (pos.first == -1) {
                        DebugText("没有合适的地点建造");
                        break;
                    }
                    HumanBuild(SN, type, pos.first, pos.second);
                }
            }
            };
        
        if(!granaryCnt1&&info.Wood>=BUILD_GRANARY_WOOD)build(BUILDING_GRANARY);
        if (farmCnt1 < 4 && info.Wood >= BUILD_FARM_WOOD && marketCnt)build(BUILDING_FARM);
        if(timer>18000&&rangeCnt1<4&&info.Wood >= BUILD_RANGE_WOOD)build(BUILDING_RANGE);
        if(timer>22000&&homeCnt1 < 12 && info.Wood >= BUILD_HOUSE_WOOD)build(BUILDING_HOME);
        if (homeCnt1 < 6 && info.Wood >= BUILD_HOUSE_WOOD)build(BUILDING_HOME);
        else if (!marketCnt1 && info.Wood >= BUILD_MARKET_WOOD)build(BUILDING_MARKET);
        //else if (arrowtowerCnt1 < 1 && info.Stone >= BUILD_ARROWTOWER_STONE && arrowTowerUnlocked)build(BUILDING_ARROWTOWER);
        else if(homeCnt1 < 9 && info.Wood >= BUILD_HOUSE_WOOD)build(BUILDING_HOME);
        else if (!armycampCnt1 && info.Wood >= BUILD_ARMYCAMP_WOOD)build(BUILDING_ARMYCAMP);
        else if (!rangeCnt1 && info.Wood >= BUILD_MARKET_WOOD && armycampCnt)build(BUILDING_RANGE);
        else if (!stableCnt1 && info.Wood >= BUILD_STABLE_WOOD && armycampCnt)build(BUILDING_STABLE);
        //
    }
    // else{
    //     assignTask(buildingFarmers,RESOURCE_TREE);
    // }
    }
    if (1) {
    // //分配工作,暂定 浆果:木头:建造:打猎:农田:石头=2:(3+6):2:6:4:1
    // auto classify = [&](int farmerSN) {
    //     static int mark = 0;
    //     if (mark % 24 <= 1)berryFarmers.push_back(farmerSN);
    //     else if (mark % 24 <= 4)woodFarmers.push_back(farmerSN);
    //     else if (mark % 24 <= 6)buildingFarmers.push_back(farmerSN);
    //     else if (mark % 24 <= 12)hunterFarmers.push_back(farmerSN);
    //     else if (mark % 24 <= 18)woodFarmers.push_back(farmerSN);
    //     else if (mark % 24 <= 22)farmFarmers.push_back(farmerSN);
    //     else stoneFarmers.push_back(farmerSN);
    //     DebugText("农民人口=");
    //     DebugText(mark + 1);
    //     DebugText("采果子人数=");
    //     DebugText((int)berryFarmers.size());
    //     DebugText("砍树人数=");
    //     DebugText((int)woodFarmers.size());
    //     DebugText("建造人数=");
    //     DebugText((int)buildingFarmers.size());
    //     DebugText("打猎人数=");
    //     DebugText((int)hunterFarmers.size());
    //     DebugText("农田人数=");
    //     DebugText((int)farmFarmers.size());
    //     DebugText("采石头人数=");
    //     DebugText((int)stoneFarmers.size());
    //     mark++;
    //     };

    // for (tagFarmer& farmer : info.farmers) {
    //     int SN = farmer.SN;
    //     bool classified = false;
    //     for (int id : berryFarmers) {
    //         if (id == SN) {
    //             classified = true;
    //             break;
    //         }
    //     }
    //     if (!classified) {
    //         for (int id : woodFarmers) {
    //             if (id == SN) {
    //                 classified = true;
    //                 break;
    //             }
    //         }
    //     }
    //     if (!classified) {
    //         for (int id : stoneFarmers) {
    //             if (id == SN) {
    //                 classified = true;
    //                 break;
    //             }
    //         }
    //     }
    //     if (!classified) {
    //         for (int id : buildingFarmers) {
    //             if (id == SN) {
    //                 classified = true;
    //                 break;
    //             }
    //         }
    //     }
    //     if (!classified) {
    //         for (int id : hunterFarmers) {
    //             if (id == SN) {
    //                 classified = true;
    //                 break;
    //             }
    //         }
    //     }
    //     if (!classified) {
    //         for (int id : farmFarmers) {
    //             if (id == SN) {
    //                 classified = true;
    //                 break;
    //             }
    //         }
    //     }
    //     if (!classified) {
    //         classify(SN);
    //         DebugText(SN);
    //     }
    // }

    // auto assignTask = [&](vector<int>farmers, int type) {
    //     for (int SN : farmers) {
    //         for (tagFarmer& farmer : info.farmers) {
    //             if (farmer.SN != SN)continue;
    //             if (farmer.NowState != HUMAN_STATE_IDLE)break;
    //             int bestResourceSN = -1;
    //             double minDist = 1e18;
    //             double dist = 1e18;
    //             for (tagResource& resource : info.resources) {
    //                 if (resource.Type != type)continue;
    //                 dist = calDistance(farmer.DR, farmer.UR, resource.DR, resource.UR);
    //                 if (dist < minDist) {
    //                     minDist = dist;
    //                     bestResourceSN = resource.SN;
    //                 }
    //             }
    //             if (bestResourceSN != -1) {
    //                 HumanAction(farmer.SN, bestResourceSN);
    //             }
    //             break;
    //         }
    //     }
    //     };
    // if(timer>18000){
    //     if(info.Meat*4<info.Gold*7){
    //         assignTask(buildingFarmers,RESOURCE_ELEPHANT);
    //         assignTask(buildingFarmers,RESOURCE_GAZELLE);
    //         assignTask(berryFarmers,RESOURCE_ELEPHANT);
    //         assignTask(berryFarmers,RESOURCE_GAZELLE);
    //         assignTask(stoneFarmers,RESOURCE_ELEPHANT);
    //         assignTask(stoneFarmers,RESOURCE_GAZELLE);
    //         assignTask(woodFarmers,RESOURCE_ELEPHANT);
    //         assignTask(woodFarmers,RESOURCE_GAZELLE);
    //     }else{
    //         assignTask(buildingFarmers,RESOURCE_GOLD);
    //         assignTask(berryFarmers,RESOURCE_GOLD);
    //         assignTask(stoneFarmers,RESOURCE_GOLD);
    //         assignTask(woodFarmers,RESOURCE_GOLD);
    //     }
    // }
    // if(timer<21750)assignTask(berryFarmers, RESOURCE_BUSH);
    // if(timer<20000)assignTask(woodFarmers, RESOURCE_TREE);
    // assignTask(stoneFarmers, RESOURCE_GOLD);
    // if(timer<22500){
    //     for (tagBuilding& building : info.buildings) {
    //         if (building.Type != BUILDING_FARM)continue;
    //         for (int SN : farmFarmers) {
    //             for (tagFarmer& farmer : info.farmers) {
    //                 if (farmer.SN != SN)continue;
    //                 if (farmer.NowState != HUMAN_STATE_IDLE)break;
    //                 HumanAction(SN, building.SN);
    //             }
    //         }
    //     }
    // }
    // bool lionFound = false;
    // for (tagResource& resource : info.resources) {
    //     if (resource.Type == RESOURCE_ELEPHANT) {
    //         lionFound = true;
    //         break;
    //     }
    // }
    // bool elephantFound = false;
    // for (tagResource& resource : info.resources) {
    //     if (resource.Type == RESOURCE_ELEPHANT) {
    //         elephantFound = true;
    //         break;
    //     }
    // }
    // //击杀优先级 象>瞪羚
    // if (hunterFarmers.size() >= 5 && elephantFound) {
    //     assignTask(hunterFarmers, RESOURCE_ELEPHANT);
    // }
    // else {
    //     assignTask(hunterFarmers, RESOURCE_GAZELLE);
    // }
    // //盖建筑
    // if(1){
    //     //记录建筑个数
    //     int homeCnt = 0, granaryCnt = 0, stockCnt = 0;
    //     int farmCnt = 0, arrowtowerCnt = 0, armycampCnt = 0;
    //     int stableCnt = 0, rangeCnt = 0, siegeCnt = 0;
    //     int marketCnt = 0, collageCnt=0;
    //     int homeCnt1 = 0, granaryCnt1 = 0, stockCnt1 = 0;
    //     int farmCnt1 = 0, arrowtowerCnt1 = 0, armycampCnt1 = 0;
    //     int stableCnt1 = 0, rangeCnt1 = 0, siegeCnt1 = 0;
    //     int marketCnt1 = 0, collageCnt1=0;
    //     for (tagBuilding& building : info.buildings) {
    //         if (building.Percent != 100)continue;
    //         switch (building.Type) {
    //         case BUILDING_HOME:homeCnt++; break;
    //         case BUILDING_GRANARY:granaryCnt++; break;
    //         case BUILDING_STOCK:stockCnt++; break;
    //         case BUILDING_FARM:farmCnt++; break;
    //         case BUILDING_ARROWTOWER:arrowtowerCnt++; break;
    //         case BUILDING_ARMYCAMP:armycampCnt++; break;
    //         case BUILDING_STABLE:stableCnt++; break;
    //         case BUILDING_RANGE:rangeCnt++; break;
    //         case BUILDING_SIEGE:siegeCnt++; break;
    //         case BUILDING_MARKET:marketCnt++; break;
    //         case BUILDING_COLLAGE:collageCnt++; break;
    //         default:break;
    //         }
    //     }
    //     for (tagBuilding& building : info.buildings) {
    //         switch (building.Type) {
    //         case BUILDING_HOME:homeCnt1++; break;
    //         case BUILDING_GRANARY:granaryCnt1++; break;
    //         case BUILDING_STOCK:stockCnt1++; break;
    //         case BUILDING_FARM:farmCnt1++; break;
    //         case BUILDING_ARROWTOWER:arrowtowerCnt1++; break;
    //         case BUILDING_ARMYCAMP:armycampCnt1++; break;
    //         case BUILDING_STABLE:stableCnt1++; break;
    //         case BUILDING_RANGE:rangeCnt1++; break;
    //         case BUILDING_SIEGE:siegeCnt1++; break;
    //         case BUILDING_MARKET:marketCnt1++; break;
    //         case BUILDING_COLLAGE:collageCnt1++; break;
    //         default:break;
    //         }
    //     }
    //     auto findOptimumPos = [&](int type) {
    //         int size = getBuildingSize(type) + 1;
    //         int bestDR = -1;
    //         int bestUR = -1;
    //         double bestScore = -1e9;
    //         for (int dr = 1; dr < MAP_L - size - 1; dr++) {
    //             for (int ur = 1; ur < MAP_U - size - 1; ur++) {
    //                 bool canBuild = true;
    //                 for (int i = 0; i < size && canBuild; i++) {
    //                     for (int j = 0; j < size && canBuild; j++) {
    //                         if (curMap[dr + i][ur + j] != 0) {
    //                             canBuild = false;
    //                             break;
    //                         }
    //                     }
    //                 }
    //                 if (!canBuild)continue;
    //                 double score = 0;
                    

    //                 if (type == BUILDING_ARROWTOWER) {
    //                     for (tagBuilding& building : info.buildings) {
    //                         if (building.Type != type)continue;
    //                         int dToSameType = abs(dr - building.BlockDR) + abs(ur - building.BlockUR);
    //                         score -= dToSameType;
    //                     }
    //                     if (abs(dr - baseBlockDR) <= 4 || abs(ur - baseBlockUR) <= 4)score -= 10;
    //                     if (abs(dr - baseBlockDR) >= 6 || abs(ur - baseBlockUR) >= 6)score -= 10;
    //                 }
    //                 else if (type == BUILDING_FARM) {
    //                     for (tagBuilding& building : info.buildings) {
    //                         if (building.Type != BUILDING_GRANARY)continue;
    //                         /*if (abs(dr - building.BlockDR) <= 3 || abs(ur - building.BlockDR) <= 3)score -= 10;
    //                         if (abs(dr - building.BlockDR) >= 6 || abs(ur - building.BlockDR) >= 6)score -= 10;
    //                         if (abs(dr - building.BlockDR) >= 10 || abs(ur - building.BlockDR) >= 10)score -= 100;*/
    //                         int dToGranary= abs(dr - building.BlockDR) + abs(ur - building.BlockUR);
    //                         score += (10 - dToGranary);
    //                     }
    //                 }
    //                 else {
    //                     int dToBase = abs(dr - baseBlockDR)+abs(ur - baseBlockUR);
    //                     score += (10 - dToBase);
    //                     /*if (abs(dr - baseBlockDR) + abs(ur - baseBlockUR) <= 3)score -= 30;
    //                     if (abs(dr - baseBlockDR) + abs(ur - baseBlockUR) >= 6)score -= 10;
    //                     if (abs(dr - baseBlockDR) + abs(ur - baseBlockUR) >= 20)score -= 50;
    //                     if (abs(dr - baseBlockDR) == 3 || abs(ur - baseBlockUR) == 3)score -= 50;*/
    //                     for (tagBuilding& building : info.buildings) {
    //                         if (building.Type != type)continue;
    //                         int dToSameType = abs(dr - building.BlockDR) + abs(ur - building.BlockUR);
    //                         score += (10 - dToSameType) * 2;
    //                     }
    //                 }
    //                 if (score > bestScore) {
    //                     bestScore = score;
    //                     bestDR = dr;
    //                     bestUR = ur;
    //                 }
    //             }
    //         }
    //         return pair<int, int>{bestDR, bestUR};
    //         };
    //     auto build = [&](int type) {
    //         vector<int>*farmers=&buildingFarmers;
    //         if (type == BUILDING_FARM) {
    //             farmers = &farmFarmers;
    //         }
    //         for (int SN : *farmers) {
    //             for (tagFarmer& farmer : info.farmers) {
    //                 if (farmer.SN != SN)continue;
    //                 if (farmer.NowState != HUMAN_STATE_IDLE)break;
    //                 pair<int, int>pos = findOptimumPos(type);
    //                 if (pos.first == -1) {
    //                     DebugText("没有合适的地点建造");
    //                     break;
    //                 }
    //                 HumanBuild(SN, type, pos.first, pos.second);
    //             }
    //         }
    //         };
        
    //     if (farmCnt1 < 4 && info.Wood >= BUILD_FARM_WOOD && marketCnt)build(BUILDING_FARM);
    //     if(info.civilizationStage==CIVILIZATION_BRONZEAGE&&collageCnt1<4&&info.Wood >= BUILD_COLLAGE_WOOD)build(BUILDING_COLLAGE);
    //     if(timer>20000&&homeCnt1 < 13 && info.Wood >= BUILD_HOUSE_WOOD)build(BUILDING_HOME);
    //     if (homeCnt1 < 6 && info.Wood >= BUILD_HOUSE_WOOD)build(BUILDING_HOME);
    //     else if (!marketCnt1 && info.Wood >= BUILD_MARKET_WOOD)build(BUILDING_MARKET);
    //     //else if (arrowtowerCnt1 < 1 && info.Stone >= BUILD_ARROWTOWER_STONE && arrowTowerUnlocked)build(BUILDING_ARROWTOWER);
    //     else if(homeCnt1 < 9 && info.Wood >= BUILD_HOUSE_WOOD)build(BUILDING_HOME);
    //     else if (!armycampCnt1 && info.Wood >= BUILD_ARMYCAMP_WOOD)build(BUILDING_ARMYCAMP);
    //     else if (!rangeCnt1 && info.Wood >= BUILD_MARKET_WOOD && armycampCnt)build(BUILDING_RANGE);
    //     else if (!stableCnt1 && info.Wood >= BUILD_STABLE_WOOD && armycampCnt)build(BUILDING_STABLE);
    //     //
    // }
    // // else{
    // //     assignTask(buildingFarmers,RESOURCE_TREE);
    // // }
    }
    //--------------------建筑工作--------------------
    if (1) {
        //进阶时代
        if (info.Meat >= 800) {
            BuildingAction(baseSN, BUILDING_CENTER_UPGRADE);
        }
        //生产村民
        if (info.farmers.size() < 24 && info.farmers.size() + info.armies.size() < info.Human_MaxNum&&timer<35000) {
            for (tagBuilding& building : info.buildings) {
                if (building.Type != BUILDING_CENTER)continue;
                if (info.Meat >= BUILDING_CENTER_CREATEFARMER_FOOD) {
                    BuildingAction(building.SN, BUILDING_CENTER_CREATEFARMER);
                }
            }
        }
        if(!logisticsResearching&&info.Gold>=BUILDING_ARMYCAMP_RESEARCH_LOGISTICS_GOLD&&info.Meat>=BUILDING_ARMYCAMP_RESEARCH_LOGISTICS_FOOD){
            logisticsResearching=true;
            for(tagBuilding&building:info.buildings){
                if(building.Type!=BUILDING_ARMYCAMP)continue;
                BuildingAction(building.SN,BUILDING_ARMYCAMP_RESEARCH_LOGISTICS);
            }
        }
        for(tagBuilding &building:info.buildings){
            if(building.Type!=BUILDING_COLLAGE)continue;
            if(info.Gold>=BUILDING_COLLAGE_CREATE_HOPLITE_GOLD&&info.Meat>=BUILDING_COLLAGE_CREATE_HOPLITE_FOOD){
                BuildingAction(building.SN,BUILDING_COLLAGE_CREATE_HOPLITE);
            }
        }
        if (wheelUnlocked) {
            //生产驷马战车
            // if (info.farmers.size() + info.armies.size() < info.Human_MaxNum) {
            //     for (tagBuilding& building : info.buildings) {
            //         if (building.Type != BUILDING_STABLE)continue;
            //         if (info.Meat >= BUILDING_STABLE_CREATE_CHARIOT_FOOD && info.Wood >= BUILDING_STABLE_CREATE_CHARIOT_WOOD) {
            //             BuildingAction(building.SN, BUILDING_STABLE_CREATE_CHARIOT);
            //         }
            //     }
            // }
            //生产战车弓兵
            if (info.farmers.size() + info.armies.size() < info.Human_MaxNum) {
                for (tagBuilding& building : info.buildings) {
                    if (building.Type != BUILDING_RANGE)continue;
                    if (info.Meat >= BUILDING_RANGE_CREATE_CHARIOT_ARCHER_FOOD && info.Wood >= BUILDING_RANGE_CREATE_CHARIOT_ARCHER_WOOD) {
                        BuildingAction(building.SN, BUILDING_RANGE_CREATE_CHARIOT_ARCHER);
                    }
                }
            }
        }
        //研发箭塔
        if (arrowTowerResearching) {
            arrowTowerResearchTimer++;
        }
        if (arrowTowerResearchTimer == 250 && !arrowTowerUnlocked) {
            arrowTowerUnlocked = true;
            arrowTowerResearching = false;
            DebugText("箭塔研发完成");
        }
        if (!arrowTowerResearching && !arrowTowerUnlocked) {
            for (tagBuilding& b : info.buildings) {
                if (b.Type != BUILDING_GRANARY)continue;
                BuildingAction(b.SN, BUILDING_GRANARY_ARROWTOWER);
                arrowTowerResearching = true;
                DebugText("开始研发箭塔");
                break;
            }
        }
        if (wheelResearching) {
            wheelResearchTimer++;
        }
        if (wheelResearchTimer == 1600 && !wheelUnlocked) {
            wheelUnlocked = true;
            wheelResearching = false;
            DebugText("车轮研发完成");
        }
        if (info.civilizationStage == CIVILIZATION_BRONZEAGE && !wheelResearching && !wheelUnlocked) {
            for (tagBuilding& b : info.buildings) {
                if (b.Type != BUILDING_MARKET)continue;
                BuildingAction(b.SN, BUILDING_MARKET_WHEEL_UPGRADE);
                wheelResearching = true;
                DebugText("开始研发车轮");
                break;
            }
        }
    }
}
}
