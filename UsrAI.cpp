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
static unordered_map<int, int> allOut_rangedLock;   // 远程兵(SN)当前锁定的目标SN(锁定狙击用)
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
static vector<int>freeFarmers;
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
static bool WOODResearching=false;
static int WOODResearchTimer=0;
static bool WOODUnlocked=false;
//--------------------发动总攻--------------------
static int allOut_time=25000;
static int diri[8]={1,1,0,-1,-1,-1,0,1};
static int dirj[8]={0,1,1,1,0,-1,-1,-1};
static int ddr[4]={1,0,-1,0};
static int dur[4]={0,1,0,-1};
unordered_map<int,int>allOut_dir;
static bool allOut_started=false;
static bool allOut_campFounded=false;
static int allOut_campDR=-1;
static int allOut_campUR=-1;
static int allOut_campSN=-1;
static unordered_set<int>destroyedTower;
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
    int bugdetWood=0;
    int bugdetMeat=0;
    int bugdetStone=0;
    int bugdetGold=0;
    int curMap[100][100] = { 0 };//-1迷雾 0可建造 1资源 2建筑 3单位 4湖泊
    bool reachable[100][100]={0};
    bool hasAnimal=false;
    bool hasBush=false;
    unordered_map<int,int>busyObject;
    set<pair<int,int>>frontier;
    if (1) {
        if (info.theMap != nullptr) {
            for (int dr = 0; dr < MAP_L; dr++) {
                for (int ur = 0; ur < MAP_U; ur++) {
                    const tagTerrain& t = (*info.theMap)[dr][ur];
                    if (t.type == MAPPATTERN_UNKNOWN)curMap[dr][ur] = -1;
                    if (t.type == MAPPATTERN_OCEAN)curMap[dr][ur]=4;
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

        for(tagBuilding& b:info.enemy_buildings){
            if(b.Type!=BUILDING_ARROWTOWER)continue;
            if((double)b.Blood/b.MaxBlood<0.5)destroyedTower.insert(b.SN);
        }

        auto scanMap=[&](){
            queue<pair<int,int>>q;
            //不能直接用基地坐标,否则出不去
            int startDR=baseBlockDR,startUR=baseBlockUR;
            bool started=false;
            for(int dr=baseBlockDR-15;dr<baseBlockDR+15&&!started;dr++){
                for(int ur=baseBlockUR-15;ur<baseBlockUR+15;ur++){
                    if(dr>=0&&dr<100&&ur>=0&&ur<100&&curMap[dr][ur]==0){
                        startDR=dr;
                        startUR=ur;
                        started=true;
                        break;
                    }
                }
            }
            reachable[startDR][startUR]=true;
            q.push({startDR,startUR});
            while(!q.empty()){
                int dr=q.front().first;
                int ur=q.front().second;
                q.pop();
                for(int i=0;i<4;i++){
                    int newDR=dr+ddr[i];
                    int newUR=ur+dur[i];
                    if(newDR<0||newDR>=100||newUR<0||newUR>=100)continue;
                    if(curMap[newDR][newUR]==-1)frontier.insert({dr,ur});
                    if(reachable[newDR][newUR])continue;
                    if(curMap[newDR][newUR]==0||curMap[newDR][newUR]==3){
                        reachable[newDR][newUR]=1;
                        q.push({newDR,newUR});
                    }
                }
            }
        };
        scanMap();

        for(tagFarmer& f:info.farmers){
            if(busyObject[f.WorkObjectSN]++);
        }

        for(tagResource& a:info.resources){
            if(a.Type!=RESOURCE_ELEPHANT&&a.Type!=RESOURCE_GAZELLE)continue;
            bool OK=false;
            for(int i=-1;i<=1&&!OK;i++){
                for(int j=-1;j<=1;j++){
                    if(a.BlockDR+i<0||a.BlockDR+i>=100||
                        a.BlockUR+j<0||a.BlockUR+j>=100)continue;
                    if(curMap[a.BlockDR+i][a.BlockUR+j]==0)OK=true;
                }
            }
            if(!OK)continue;
            if(calDistance(a.DR,a.UR,allOut_campDR*BLOCKSIDELENGTH,allOut_campUR*BLOCKSIDELENGTH)/BLOCKSIDELENGTH<15)continue;
            hasAnimal=true;
        }
        for(tagResource& r:info.resources){
            if(r.Type!=RESOURCE_BUSH)continue;
            if(calDistance(r.DR,r.UR,allOut_campDR*BLOCKSIDELENGTH,allOut_campUR*BLOCKSIDELENGTH)/BLOCKSIDELENGTH<15)continue;
            hasBush=true;
        }
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
        if (coolDownWhenAttacked > 0)coolDownWhenAttacked--;

        if (attackInComing && !allOut_started) {
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
                        HumanMove(priestSN, cDR * BLOCKSIDELENGTH,cUR * BLOCKSIDELENGTH);
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
                    if (!nearBase && !(allOut_campFounded && info.enemy_armies.empty() && info.enemy_farmers.empty())) {
                        priestIdleMoveTimer++;
                        if (priestIdleMoveTimer % 25 == 1) {
                            if (allOut_campFounded) {
                                int hDR = allOut_campDR - 12, hUR = allOut_campUR - 3;
                                if (!retreatFree(hDR, hUR)) { hDR = allOut_campDR + 3; hUR = allOut_campUR + 12; }
                                if (!retreatFree(hDR, hUR)) { hDR = allOut_campDR; hUR = allOut_campUR + 8; }
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

        if (!canConvert && coolDownWhenAttacked == 0 && !allOut_started) {
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
                    if (weight == 0 && !(allOut_campFounded && info.enemy_armies.empty() && info.enemy_farmers.empty()))goHome = true;  // 敌方单位已清时交给祭司转建筑,不回家
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
            if (!allOut_started && abs(army.BlockDR - baseBlockDR) + abs(army.BlockUR - baseBlockUR) > 20) {
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
                        if (army.NowState != HUMAN_STATE_IDLE)continue;
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
                bool hasStoneThrower=false;
                for(tagArmy& e:info.enemy_armies){
                    if(e.Sort==AT_STONE_THROWER){
                        for (tagArmy& a : info.armies) {
                            if (a.SN == priestSN)continue;
                            if (a.NowState != HUMAN_STATE_IDLE)continue;
                            hasStoneThrower=true;
                            HumanAction(a.SN, e.SN);
                        }
                    }
                }
                if(!hasStoneThrower){
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

        // 祭司探图定时:5分半(8250 tick)~8分钟(12000 tick)为强制探图窗口,
        // 不受敌袭标志(attackInComing≈3.7分钟就置真)阻断;8分半后停探回家。
        if (!allOut_started && baseFound) {
            // if (timer >= 8250 && timer < 12000) {
            //     needExploration = true;                 // 定时探图窗口内持续探图
            // }
            if (timer >= 12000) {
                needExploration = false;                // 8分钟后停探
                if (priestState == HUMAN_STATE_IDLE &&
                    abs(priestBlockDR - baseBlockDR) + abs(priestBlockUR - baseBlockUR) > 4) {
                    HumanMove(priestSN, (baseBlockDR - 1) * BLOCKSIDELENGTH,
                                         (baseBlockUR - 1) * BLOCKSIDELENGTH);   // 回家
                }
            }
        }

        bool exploreWindow = false;
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
    if (1) {
        bool hasMelee=false;
        bool hasStoneThrower=false;
        int stoneThrowerSN=-1;
        auto isRanged=[&](int sort){
            switch(sort)
            {
                case AT_CLUBMAN:
                case AT_SCOUT:
                case AT_SWORDSMAN:
                case AT_CAVALRY:
                case AT_HOPLITE:
                case AT_CHARIOT:
                case AT_BROADSWORDSMAN:
                return false;
                break;
                case AT_SLINGER:
                case AT_BOWMAN:
                case AT_IMPROVED:
                case AT_PRIEST:
                case AT_CHARIOT_ARCHER:
                case AT_COMPOSITE_BOWMAN:
                return true;
                default:
                return true;
                break;
            }
        };
        if(1){
            for(tagArmy& e:info.enemy_armies){
                if(!isRanged(e.Sort)){
                    hasMelee=true;
                    break;
                }
            }
            for(tagArmy& e:info.enemy_armies){
                if(e.Sort==AT_STONE_THROWER){
                    hasStoneThrower=true;
                    stoneThrowerSN=e.SN;
                    break;
                }
            }
        }
        auto allOut_go=[&](tagArmy& army){
            int bestD=1e9;
            pair<int,int>dest={-1,-1};
            for(auto f:frontier){
                int d=abs(army.BlockDR-f.first)+abs(army.BlockUR-f.second);
                if(d<bestD){
                    bestD=d;
                    dest={f.first,f.second};
                }
            }
            if(dest.first==-1){
                dest={baseBlockDR-1,baseBlockUR-1};
                DebugText("俺找不到边界,只能往家走了...");
            }
            HumanMove(army.SN,dest.first*BLOCKSIDELENGTH,dest.second*BLOCKSIDELENGTH);
        };
        auto allOut_escape=[&](tagArmy& army){
            int searchRange=10;
            if(army.Sort==AT_PRIEST)searchRange=10;
            int bestDR = -1;
            int bestUR = -1;
            double bestScore = 0;
            int startDR = max(army.BlockDR - searchRange, 2);
            int startUR = max(army.BlockUR - searchRange, 2);
            int endDR = min(army.BlockDR + searchRange, MAP_L - 2);
            int endUR = min(army.BlockUR + searchRange, MAP_U - 2);
            for (int dr = startDR; dr <= endDR; dr++) {
                for (int ur = startUR; ur <= endUR; ur++) {
                    double score = 0;
                    if (curMap[dr][ur]!=0)continue;
                    for (tagArmy& enemy : info.enemy_armies) {
                        double dToEnemy = calDistance(army.DR, army.UR, enemy.DR, enemy.UR);
                        if (dToEnemy > searchRange * BLOCKSIDELENGTH)continue;
                        score += abs(dr - enemy.BlockDR) + abs(ur - enemy.BlockUR);
                    }
                    if (score > bestScore) {
                        bestScore = score;
                        bestDR = dr;
                        bestUR = ur;
                    }
                }
            }
            if (bestDR != -1 && bestUR != -1) {
                HumanMove(army.SN, bestDR * BLOCKSIDELENGTH, bestUR * BLOCKSIDELENGTH);
            }
        };   
        auto smartAttack=[&](tagArmy& army){
            int locked=-1;
            auto it=allOut_rangedLock.find(army.SN);
            if(it!=allOut_rangedLock.end())locked=it->second;
            bool valid=false;
            if(locked!=-1){
                for(tagArmy& e:info.enemy_armies){
                    if(e.SN!=locked)continue;
                    if(calDistance(army.DR,army.UR,e.DR,e.UR)<=8*BLOCKSIDELENGTH){
                        valid=true;
                        if(army.NowState==HUMAN_STATE_IDLE)HumanAction(army.SN,e.SN);
                    }
                }
            }
            bool danger=false;
            if(isRanged(army.Sort)){
                int awarenessRange=2;
                if(army.Sort==AT_PRIEST||army.Sort==AT_STONE_THROWER)awarenessRange=5;
                for(tagArmy& e:info.enemy_armies){
                    double d=calDistance(army.DR,army.UR,e.DR,e.UR);
                    if(d<=awarenessRange*BLOCKSIDELENGTH){
                        danger=true;
                        if(army.NowState!=HUMAN_STATE_WALKING)allOut_escape(army);
                        break;
                    }
                }
            }

            if((locked==-1||!valid)&&!danger){
                if(hasMelee||(!hasMelee&&!hasStoneThrower)){
                    double bestD=1e9;
                    int bestSN=-1;
                    for(tagArmy& e:info.enemy_armies){
                        double d=calDistance(army.DR,army.UR,e.DR,e.UR);
                        if(d<bestD){
                            bestD=d;
                            bestSN=e.SN;
                        }
                    }
                    allOut_rangedLock[army.SN]=bestSN;
                    if(army.NowState!=HUMAN_STATE_WALKING&&bestSN!=-1)HumanAction(army.SN,bestSN);
                }else{
                    allOut_rangedLock[army.SN]=stoneThrowerSN;
                    if(army.NowState!=HUMAN_STATE_WALKING&&stoneThrowerSN!=-1)HumanAction(army.SN,stoneThrowerSN);
                }
            }
        };
        auto attackTower=[&](tagArmy& army){
            double bestD=1e9;
            int bestSN=-1;
            for(tagBuilding& b:info.enemy_buildings){
                if(b.Type!=BUILDING_ARROWTOWER)continue;
                double d=calDistance(army.DR,army.UR,b.BlockDR*BLOCKSIDELENGTH,b.BlockUR*BLOCKSIDELENGTH);
                if(d<bestD){
                    bestD=d;
                    bestSN=b.SN;
                }
            }
            HumanAction(army.SN,bestSN);
        };
        if(timer>=allOut_time&&!allOut_started)allOut_started=true;

        if(allOut_started&&timer%19==0){
            if(!allOut_campFounded){
                for(tagArmy& a : info.armies){
                    //if(a.Sort==AT_PRIEST)continue;
                    allOut_go(a);
                }
                for(tagBuilding& b:info.enemy_buildings){
                    if(b.Type!=BUILDING_SIEGE)continue;
                    allOut_campFounded=true;
                    allOut_campDR=b.BlockDR;
                    allOut_campUR=b.BlockUR;
                    allOut_campSN=b.SN;
                    DebugText("敌人基地找到啦,快撤出去!!!");
                    for(tagArmy& a:info.armies){
                        HumanMove(a.SN,baseBlockDR*BLOCKSIDELENGTH,baseBlockUR*BLOCKSIDELENGTH);
                    }
                }
            }else if(allOut_campFounded){
                bool enemyArrowTowerExist=false;
                for(tagBuilding& b:info.enemy_buildings){
                    if(b.Type==BUILDING_ARROWTOWER){
                        enemyArrowTowerExist=true;
                    }
                }
                for(tagArmy& a:info.armies){
                    //视野之内没敌人,往敌方基地赶
                    if(info.enemy_armies.empty()){
                        if(enemyArrowTowerExist&&a.NowState!=HUMAN_STATE_ATTACKING){
                            if(a.Sort==AT_PRIEST||timer%38!=0)continue;
                            attackTower(a);
                        }else if(!enemyArrowTowerExist){
                            if(a.Sort==AT_PRIEST)continue;
                            HumanMove(a.SN,allOut_campDR*BLOCKSIDELENGTH,allOut_campUR*BLOCKSIDELENGTH);
                        }
                        if(destroyedTower.size()>=4){
                            if(priestState==HUMAN_STATE_IDLE){
                                HumanAction(priestSN,allOut_campSN);
                                DebugText("敌人全灭,箭塔几乎全灭,祭司转化siege");
                            }
                        }
                    }
                    else{
                        if(a.Sort==AT_PRIEST&&(hasMelee||priestState==HUMAN_STATE_ATTACKING||!canConvert))continue;
                        //战车弓兵的攻击间隔是1.5秒,即37.5帧
                        if(a.Sort==AT_CHARIOT_ARCHER&&timer%38!=0)continue;
                        //5秒,125帧
                        if(a.Sort==AT_STONE_THROWER&&timer%133!=0)continue;
                        smartAttack(a);
                    }
                }
            }
        }
    }
    //--------------------农民工作--------------------
    if (1) {
    //分配工作,暂定 浆果:木头:建造:打猎:农田:石头=2:(3+8):2:6:2:1
    auto classify = [&](int farmerSN) {
        static int mark = 0;
        if (mark % 24 <= 1){berryFarmers.push_back(farmerSN);freeFarmers.push_back(farmerSN);}
        else if (mark % 24 <= 4){woodFarmers.push_back(farmerSN);freeFarmers.push_back(farmerSN);}
        else if (mark % 24 <= 6){buildingFarmers.push_back(farmerSN);freeFarmers.push_back(farmerSN);}
        else if (mark % 24 <= 12){hunterFarmers.push_back(farmerSN);freeFarmers.push_back(farmerSN);}
        else if (mark % 24 <= 20){woodFarmers.push_back(farmerSN);freeFarmers.push_back(farmerSN);}
        else if (mark % 24 <= 22)farmFarmers.push_back(farmerSN);
        else {stoneFarmers.push_back(farmerSN);freeFarmers.push_back(farmerSN);}
        // DebugText("农民人口=");
        // DebugText(mark + 1);
        // DebugText("采果子人数=");
        // DebugText((int)berryFarmers.size());
        // DebugText("砍树人数=");
        // DebugText((int)woodFarmers.size());
        // DebugText("建造人数=");
        // DebugText((int)buildingFarmers.size());
        // DebugText("打猎人数=");
        // DebugText((int)hunterFarmers.size());
        // DebugText("农田人数=");
        // DebugText((int)farmFarmers.size());
        // DebugText("采石头人数=");
        // DebugText((int)stoneFarmers.size());
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
        }
    }

    auto assignTask = [&](vector<int>farmers, int type) {
        for (int SN : farmers) {
            for (tagFarmer& farmer : info.farmers) {
                if (farmer.SN != SN)continue;
                if(farmer.NowState!=HUMAN_STATE_IDLE)continue;
                int bestResourceSN = -1;
                double minDist = 1e18;
                double dist = 1e18;
                for (tagResource& resource : info.resources) {
                    if (resource.Type != type)continue;
                    if(type==RESOURCE_TREE){
                        if(busyObject.count(resource.SN))continue;
                        bool OK=false;
                        for(int i=-1;i<=1&&!OK;i++){
                            for(int j=-1;j<=1;j++){
                                if(resource.BlockDR+i<0||resource.BlockDR+i>=100||
                                    resource.BlockUR+j<0||resource.BlockUR+j>=100)continue;
                                if(curMap[resource.BlockDR+i][resource.BlockUR+j]==0)OK=true;
                            }
                        }
                        if(!OK)continue;
                    }
                    if(resource.Type==RESOURCE_TREE){
                        for(tagBuilding& b:info.buildings){
                            if(b.Type!=BUILDING_STOCK)continue;
                            dist=calDistance(b.BlockDR*BLOCKSIDELENGTH,b.BlockUR*BLOCKSIDELENGTH,resource.DR,resource.UR);
                        }
                    }else if(resource.Type==RESOURCE_BUSH){
                        for(tagBuilding& b:info.buildings){
                            if(b.Type!=BUILDING_GRANARY)continue;
                            dist=calDistance(b.BlockDR*BLOCKSIDELENGTH,b.BlockUR*BLOCKSIDELENGTH,resource.DR,resource.UR);
                        }
                    }else {
                        dist = calDistance(farmer.DR, farmer.UR, resource.DR, resource.UR);
                    }
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
                        int dToGranary= abs(dr - building.BlockDR) + abs(ur - building.BlockUR);
                        score += (10 - dToGranary);
                    }
                }
                else if(type==BUILDING_STOCK){
                    for(tagResource& a:info.resources){
                        if(a.Type!=RESOURCE_GAZELLE&&a.Type!=RESOURCE_ELEPHANT)continue;
                        double d=calDistance(a.DR,a.UR,dr*BLOCKSIDELENGTH,ur*BLOCKSIDELENGTH)/BLOCKSIDELENGTH;
                        if(d>15)continue;
                        score+=(15-d);
                    }
                }
                else {
                    int dToBase = abs(dr - baseBlockDR)+abs(ur - baseBlockUR);
                    score += (10 - dToBase);
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
            if(!hasAnimal&&!hasBush)farmers=&freeFarmers;
        }else if(type == BUILDING_STOCK){
            farmers = &hunterFarmers;
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
                if(type==BUILDING_FARM){
                    DebugText("我是龙民,我要耕地!");
                    DebugText(farmer.SN);
                }
                return;
            }
        }
        };
    auto hunt=[&](){
        double bestScore=-1e9;
        int bestSN=-1;
        double itsDR=-1;
        double itsUR=-1;
        int stockDR=-1;
        int stockUR=-1;
        bool needNewStock=true;
        bool needHelp=false;
        int needHelpSN=-1;
        for(tagBuilding& b:info.buildings){
            if(b.Type!=BUILDING_STOCK)continue;
            stockDR=b.BlockDR;
            stockUR=b.BlockUR;
            break;
        }
        if(timer>18000){
            // for(tagResource& a:info.resources){
            //     if(a.Type!=RESOURCE_ELEPHANT&&a.Type!=RESOURCE_GAZELLE)continue;
            //     double score=0;
            //     double d=calDistance(a.DR,a.UR,stockDR*BLOCKSIDELENGTH,stockUR*BLOCKSIDELENGTH)/BLOCKSIDELENGTH;
            //     if(a.Type==RESOURCE_ELEPHANT){
            //         score-=(d+5);
            //         if(busyObject.count(a.SN)){
            //             score+=10;
            //             if(busyObject[a.SN]>6)score-=30;
            //         }
            //     }
            //     if(a.Type==RESOURCE_GAZELLE){
            //         score-=d;
            //         if(busyObject.count(a.SN)&&busyObject[a.SN]>3)score-=15;
            //     }
            //     if(score>bestScore){
            //         bestScore=score;
            //         bestSN=a.SN;
            //     }
            // }
            // for(int sn:freeFarmers){
            //     for(tagFarmer& f:info.farmers){
            //         if(f.SN!=sn)continue;
            //         if(f.NowState!=HUMAN_STATE_IDLE)break;
            //         HumanAction(sn,bestSN);
            //         return;
            //     }
            // }
            //////////////////////////////////////
            for(tagResource& a:info.resources){
                if(a.Type!=RESOURCE_ELEPHANT&&a.Type!=RESOURCE_GAZELLE)continue;
                bool OK=false;
                for(int i=-1;i<=1&&!OK;i++){
                    for(int j=-1;j<=1;j++){
                        if(a.BlockDR+i<0||a.BlockDR+i>=100||
                            a.BlockUR+j<0||a.BlockUR+j>=100)continue;
                        if(curMap[a.BlockDR+i][a.BlockUR+j]==0)OK=true;
                    }
                }
                if(!OK)continue;
                for(tagBuilding& b:info.buildings){
                    if(b.Type!=BUILDING_STOCK&&b.Type!=BUILDING_CENTER)continue;
                    double score=0;
                    double d=calDistance(a.DR,a.UR,b.BlockDR*BLOCKSIDELENGTH,b.BlockUR*BLOCKSIDELENGTH)/BLOCKSIDELENGTH;
                    if(a.Type==RESOURCE_ELEPHANT){
                        score-=(d+5);
                        if(busyObject.count(a.SN)){
                            score+=10;
                            if(busyObject[a.SN]>6)score-=30;
                        }
                    }
                    if(a.Type==RESOURCE_GAZELLE){
                        score-=d;
                        if(busyObject.count(a.SN)&&busyObject[a.SN]>3)score-=15;
                    }
                    if(score>bestScore){
                        bestScore=score;
                        bestSN=a.SN;
                        itsDR=a.DR;
                        itsUR=a.UR;
                    }
                }
            }
            if(bestSN==-1||itsDR==-1||itsUR==-1)return;
            for(tagBuilding& b:info.buildings){
                if(b.Type!=BUILDING_STOCK&&b.Type!=BUILDING_CENTER)continue;
                double d=calDistance(itsDR,itsUR,b.BlockDR*BLOCKSIDELENGTH,b.BlockUR*BLOCKSIDELENGTH)/BLOCKSIDELENGTH;
                if(d<15){
                    needNewStock=false;
                    if(b.Percent<100){
                        needHelp=true;
                        needHelpSN=b.SN;
                    }
                    break;
                }
            }
            for(int sn:freeFarmers){
                for(tagFarmer& f:info.farmers){
                    if(f.SN!=sn)continue;
                    if(f.NowState!=HUMAN_STATE_IDLE)break;
                    if(needNewStock&&info.Wood-bugdetWood>=BUILD_STOCK_WOOD){
                        build(BUILDING_STOCK);
                        bugdetWood+=BUILD_STOCK_WOOD;
                    }else if(needHelp){
                        HumanAction(sn,needHelpSN);
                    }
                    else{
                        HumanAction(sn,bestSN);
                    }
                    return;
                }
            }
        }else{
            for(tagResource& a:info.resources){
                if(a.Type!=RESOURCE_ELEPHANT&&a.Type!=RESOURCE_GAZELLE)continue;
                bool OK=false;
                for(int i=-1;i<=1&&!OK;i++){
                    for(int j=-1;j<=1;j++){
                        if(a.BlockDR+i<0||a.BlockDR+i>=100||
                            a.BlockUR+j<0||a.BlockUR+j>=100)continue;
                        if(curMap[a.BlockDR+i][a.BlockUR+j]==0)OK=true;
                    }
                }
                if(!OK)continue;
                for(tagBuilding& b:info.buildings){
                    if(b.Type!=BUILDING_STOCK&&b.Type!=BUILDING_CENTER)continue;
                    double score=0;
                    double d=calDistance(a.DR,a.UR,b.BlockDR*BLOCKSIDELENGTH,b.BlockUR*BLOCKSIDELENGTH)/BLOCKSIDELENGTH;
                    if(a.Type==RESOURCE_ELEPHANT){
                        score-=(d+5);
                        if(hunterFarmers.size()<5)score-=100;
                        if(busyObject.count(a.SN))score+=10;
                    }
                    if(a.Type==RESOURCE_GAZELLE){
                        score-=d;
                        if(busyObject.count(a.SN)&&busyObject[a.SN]>3)score-=15;
                    }
                    if(score>bestScore){
                        bestScore=score;
                        bestSN=a.SN;
                        itsDR=a.DR;
                        itsUR=a.UR;
                    }
                }
            }
            if(bestSN==-1||itsDR==-1||itsUR==-1)return;
            for(tagBuilding& b:info.buildings){
                if(b.Type!=BUILDING_STOCK&&b.Type!=BUILDING_CENTER)continue;
                double d=calDistance(itsDR,itsUR,b.BlockDR*BLOCKSIDELENGTH,b.BlockUR*BLOCKSIDELENGTH)/BLOCKSIDELENGTH;
                if(d<15){
                    needNewStock=false;
                    if(b.Percent<100){
                        needHelp=true;
                        needHelpSN=b.SN;
                    }
                    break;
                }
            }
            for(int sn:hunterFarmers){
                for(tagFarmer& f:info.farmers){
                    if(f.SN!=sn)continue;
                    if(f.NowState!=HUMAN_STATE_IDLE)break;
                    if(needNewStock&&info.Wood-bugdetWood>=BUILD_STOCK_WOOD){
                        build(BUILDING_STOCK);
                        bugdetWood+=BUILD_STOCK_WOOD;
                    }else if(needHelp){
                        HumanAction(sn,needHelpSN);
                    }
                    else{
                        HumanAction(sn,bestSN);
                    }
                    return;
                }
            }
        }
    };
    if(timer>18000){
        if(info.Meat*7<info.Wood*4){
            if(hasAnimal)hunt();
            else{
                assignTask(freeFarmers,RESOURCE_BUSH);
            }
        }else{
            assignTask(freeFarmers,RESOURCE_TREE);
        }
    }
    else {
        if(hasAnimal)hunt();
        else if(hasBush){
            assignTask(hunterFarmers,RESOURCE_BUSH);
        }else{
            build(BUILDING_FARM);
        }
    }
    if(timer<21750)assignTask(berryFarmers, RESOURCE_BUSH);
    if(timer<20000)assignTask(woodFarmers, RESOURCE_TREE);
    if(timer<18000)assignTask(stoneFarmers, RESOURCE_GOLD);
    //盖建筑
    if(1){
        bool turnForMe=true;
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
        
        if(!granaryCnt1&&info.Wood-bugdetWood>=BUILD_GRANARY_WOOD){build(BUILDING_GRANARY);bugdetWood+=BUILD_GRANARY_WOOD;}
        if (farmCnt1 < 4 && info.Wood-bugdetWood >= BUILD_FARM_WOOD && marketCnt&&WOODUnlocked){build(BUILDING_FARM);bugdetWood+=BUILD_FARM_WOOD;}
        if(timer>18000&&rangeCnt1<4&&info.Wood-bugdetWood >= BUILD_RANGE_WOOD){build(BUILDING_RANGE);bugdetWood+=BUILD_RANGE_WOOD;}
        if(timer>16000&&homeCnt1<12 && info.Wood-bugdetWood >= BUILD_HOUSE_WOOD){build(BUILDING_HOME);bugdetWood+=BUILD_HOUSE_WOOD;}
        if (homeCnt1 < 6 && info.Wood-bugdetWood >= BUILD_HOUSE_WOOD){
            build(BUILDING_HOME);
            bugdetWood+=BUILD_HOUSE_WOOD;
            turnForMe=false;
        }
        else if (turnForMe&&!marketCnt1){
            if(info.Wood-bugdetWood >= BUILD_MARKET_WOOD){build(BUILDING_MARKET);bugdetWood+=BUILD_MARKET_WOOD;}
            turnForMe=false;
        } 
        else if(turnForMe&&homeCnt1<9){
            if(info.Wood-bugdetWood >= BUILD_HOUSE_WOOD&&WOODUnlocked){build(BUILDING_HOME);bugdetWood+=BUILD_HOUSE_WOOD;}
            turnForMe=false;
        }
        else if (turnForMe&&!armycampCnt1){
            if(info.Wood-bugdetWood >= BUILD_ARMYCAMP_WOOD&&WOODUnlocked){build(BUILDING_ARMYCAMP);bugdetWood+=BUILD_ARMYCAMP_WOOD;}
            turnForMe=false;
        } 
        else if (turnForMe&&!rangeCnt1){
            if(info.Wood-bugdetWood >= BUILD_RANGE_WOOD && armycampCnt&&WOODUnlocked){build(BUILDING_RANGE);bugdetWood+=BUILD_RANGE_WOOD;}
            turnForMe=false;
        }
        else if (turnForMe&&!stableCnt1){
            if(info.Wood-bugdetWood >= BUILD_STABLE_WOOD && armycampCnt&&WOODUnlocked){build(BUILDING_STABLE);bugdetWood+=BUILD_STABLE_WOOD;}
        }
    }
    }
    //--------------------建筑工作--------------------
    if (1) {
        //进阶时代
        if (info.Meat-bugdetMeat >= 800) {
            BuildingAction(baseSN, BUILDING_CENTER_UPGRADE);
            bugdetMeat+=800;
        }
        //生产村民
        if (info.farmers.size() < 24 && info.farmers.size() + info.armies.size() < info.Human_MaxNum&&timer<35000) {
            if(info.farmers.size()<18||WOODUnlocked){
                for (tagBuilding& building : info.buildings) {
                    if (building.Type != BUILDING_CENTER)continue;
                    if (info.Meat-bugdetMeat >= BUILDING_CENTER_CREATEFARMER_FOOD) {
                        BuildingAction(building.SN, BUILDING_CENTER_CREATEFARMER);
                        bugdetMeat+=BUILDING_CENTER_CREATEFARMER_FOOD;
                    }
                }
            }
        }
        if(!logisticsResearching&&info.Gold-bugdetGold>=BUILDING_ARMYCAMP_RESEARCH_LOGISTICS_GOLD&&info.Meat-bugdetMeat>=BUILDING_ARMYCAMP_RESEARCH_LOGISTICS_FOOD){
            logisticsResearching=true;
            for(tagBuilding&building:info.buildings){
                if(building.Type!=BUILDING_ARMYCAMP)continue;
                BuildingAction(building.SN,BUILDING_ARMYCAMP_RESEARCH_LOGISTICS);
                bugdetGold+=BUILDING_ARMYCAMP_RESEARCH_LOGISTICS_GOLD;
                bugdetMeat+=BUILDING_ARMYCAMP_RESEARCH_LOGISTICS_FOOD;
            }
        }
        for(tagBuilding &building:info.buildings){
            if(building.Type!=BUILDING_COLLAGE)continue;
            if(info.Gold-bugdetGold>=BUILDING_COLLAGE_CREATE_HOPLITE_GOLD&&info.Meat-bugdetMeat>=BUILDING_COLLAGE_CREATE_HOPLITE_FOOD){
                BuildingAction(building.SN,BUILDING_COLLAGE_CREATE_HOPLITE);
                bugdetGold+=BUILDING_COLLAGE_CREATE_HOPLITE_GOLD;
                bugdetMeat+=BUILDING_COLLAGE_CREATE_HOPLITE_FOOD;
            }
        }
        if (wheelUnlocked) {
            //生产战车弓兵
            if (info.farmers.size() + info.armies.size() < info.Human_MaxNum) {
                for (tagBuilding& building : info.buildings) {
                    if (building.Type != BUILDING_RANGE)continue;
                    if (info.Meat-bugdetMeat >= BUILDING_RANGE_CREATE_CHARIOT_ARCHER_FOOD && info.Wood-bugdetWood >= BUILDING_RANGE_CREATE_CHARIOT_ARCHER_WOOD) {
                        BuildingAction(building.SN, BUILDING_RANGE_CREATE_CHARIOT_ARCHER);
                        bugdetMeat+=BUILDING_RANGE_CREATE_CHARIOT_ARCHER_FOOD;
                        bugdetWood+=BUILDING_RANGE_CREATE_CHARIOT_ARCHER_WOOD;
                    }
                }
            }
        }
        //研发箭塔
        // if (arrowTowerResearching) {
        //     arrowTowerResearchTimer++;
        // }
        // if (arrowTowerResearchTimer == 250 && !arrowTowerUnlocked) {
        //     arrowTowerUnlocked = true;
        //     arrowTowerResearching = false;
        //     DebugText("箭塔研发完成");
        // }
        // if (!arrowTowerResearching && !arrowTowerUnlocked) {
        //     for (tagBuilding& b : info.buildings) {
        //         if (b.Type != BUILDING_GRANARY)continue;
        //         if(info.Meat-bugdetMeat>=BUILDING_GRANARY_ARROWTOWER_FOOD){
        //             BuildingAction(b.SN, BUILDING_GRANARY_ARROWTOWER);
        //             bugdetMeat+=BUILDING_GRANARY_ARROWTOWER_FOOD;
        //             arrowTowerResearching = true;
        //             DebugText("开始研发箭塔");
        //         }
        //         break;
        //     }
        // }
        if(WOODResearching){
            WOODResearchTimer++;
        }
        if(WOODResearchTimer==1000&&!WOODUnlocked){
            WOODUnlocked=true;
            WOODResearching=false;
            DebugText("木材加工研发完成");
        }
        if(!WOODResearching&&!WOODUnlocked){
            for(tagBuilding& b:info.buildings){
                if(b.Type!=BUILDING_MARKET)continue;
                if(info.Wood-bugdetWood>=BUILDING_MARKET_WOOD_UPGRADE_WOOD&&
                    info.Meat-bugdetMeat>=BUILDING_MARKET_WOOD_UPGRADE_FOOD){
                        BuildingAction(b.SN,BUILDING_MARKET_WOOD_UPGRADE);
                        bugdetWood+=BUILDING_MARKET_WOOD_UPGRADE_WOOD;
                        bugdetMeat+=BUILDING_MARKET_WOOD_UPGRADE_FOOD;
                        WOODResearching=true;
                    DebugText("开始研发木材加工");
                }
                break;
            }
        }
        if (wheelResearching) {
            wheelResearchTimer++;
        }
        if (wheelResearchTimer == 1000 && !wheelUnlocked) {
            wheelUnlocked = true;
            wheelResearching = false;
            DebugText("车轮研发完成");
        }
        if (info.civilizationStage == CIVILIZATION_BRONZEAGE && !wheelResearching && !wheelUnlocked) {
            for (tagBuilding& b : info.buildings) {
                if (b.Type != BUILDING_MARKET)continue;
                if(info.Wood-bugdetWood>=BUILDING_MARKET_WHEEL_UPGRADE_WOOD&&
                    info.Meat-bugdetMeat>=BUILDING_MARKET_WHEEL_UPGRADE_FOOD){
                        BuildingAction(b.SN, BUILDING_MARKET_WHEEL_UPGRADE);
                        bugdetWood+=BUILDING_MARKET_WHEEL_UPGRADE_WOOD;
                        bugdetMeat+=BUILDING_MARKET_WHEEL_UPGRADE_FOOD;
                        wheelResearching = true;
                        DebugText("开始研发车轮");
                }
                break;
            }
        }
    }
}
