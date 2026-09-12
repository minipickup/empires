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
//--------------------祭司探图--------------------
//大本营坐标
static bool baseFound = false;
static int baseSN = -1;
static int baseBlockDR = 50;
static int baseBlockUR = 50;
//探图祭祀信息
static int priestSN = -1;
static double priestDR = 50;
static double priestUR = 50;
static int priestBlockDR = 50;
static int priestBlockUR = 50;
static bool canConvert = false;
static int priestState = -1;
static int priestWorkObejctSN = -1;
static int coolDown = 0;
static bool needExploration = true;
//--------------------农民工作--------------------
static vector<int>freeFarmers;
static vector<int>foodFarmers;
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
static bool craftResearching=false;
static int craftResearchTimer=0;
static bool craftUnlocked=false;
//--------------------发动总攻--------------------
static int allOut_time=25000;
static int ddr[4]={1,0,-1,0};
static int dur[4]={0,1,0,-1};
static unordered_map<int, int>allOut_rangedLock;
static bool allOut_started=false;
static bool allOut_campFounded=false;
static int allOut_campDR=-1e9;
static int allOut_campUR=-1e9;
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
int getVision(int sort){
    switch(sort){
        case AT_CHARIOT_ARCHER:
        case AT_COMPOSITE_BOWMAN:
        return 11;
        break;
        case AT_PRIEST:
        return 12;
        break;
        case AT_STONE_THROWER:
        return 13;
        break;
        default:
        return 4;
    }
}
bool isRanged(int sort){
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
    timer++;
    int bugdetWood=0;
    int bugdetMeat=0;
    int bugdetStone=0;
    int bugdetGold=0;
    int curMap[100][100];//-1迷雾 0可建造 1资源 2建筑 3单位 4湖泊
    bool reachable[100][100];
    bool hasAnimal=false;
    bool hasBush=false;
    unordered_map<int,int>busyObject;
    set<pair<int,int>>frontier;
    bool visableBlock[100][100];
    if (1) {
        for(int i=0;i<100;i++){
            for(int j=0;j<100;j++){
                curMap[i][j]=0;
                reachable[i][j]=0;
                visableBlock[i][j]=0;
            }
        }
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
            if(calDistance(a.DR,a.UR,allOut_campDR*BLOCKSIDELENGTH,allOut_campUR*BLOCKSIDELENGTH)/BLOCKSIDELENGTH<25)continue;
            hasAnimal=true;
            break;
        }
        for(tagResource& r:info.resources){
            if(r.Type!=RESOURCE_BUSH)continue;
            if(busyObject.count(r.SN)&&busyObject[r.SN]>2)continue;
            if(calDistance(r.DR,r.UR,allOut_campDR*BLOCKSIDELENGTH,allOut_campUR*BLOCKSIDELENGTH)/BLOCKSIDELENGTH<25)continue;
            hasBush=true;
            break;
        }
        for(tagArmy& a:info.armies){
            int vision=getVision(a.Sort);
            int startDR=max(a.BlockDR-vision,0);
            int startUR=max(a.BlockUR-vision,0);
            int endDR=min(a.BlockDR+vision,100);
            int endUR=min(a.BlockUR+vision,100);
            for(int dr=startDR;dr<endDR;dr++){
                for(int ur=startUR;ur<endUR;ur++){
                    double d=calDistance(a.DR,a.UR,dr*BLOCKSIDELENGTH,ur*BLOCKSIDELENGTH)/BLOCKSIDELENGTH;
                    if(d>vision)continue;
                    visableBlock[dr][ur]=1;
                }
            }
        }
    }
    //更新祭祀信息
    for (tagArmy& a : info.armies) {
        if(a.Sort!=AT_PRIEST)continue;
        priestSN=a.SN;
        priestDR = a.DR;
        priestUR = a.UR;
        priestBlockDR = priestDR / BLOCKSIDELENGTH;
        priestBlockUR = priestUR / BLOCKSIDELENGTH;
        priestState = a.NowState;
        priestWorkObejctSN = a.WorkObjectSN;
        canConvert=a.ConvertCooldown==0?true:false;
        break;
    }
    //--------------------敌袭应对--------------------
    if (1) {
        int enemyAttackingPriestSN=-1;
        auto getThreatEnemySN=[&](){
            double bestScore=-1e9;
            int bestSN=-1;
            for(tagArmy& e:info.enemy_armies){
                double score=0;
                if(e.WorkObjectSN==priestSN){
                    score+=100;
                    double d=calDistance(e.DR,e.UR,priestDR,priestUR)/BLOCKSIDELENGTH;
                    score-=d;
                }
                if(e.Sort==AT_CHARIOT||e.Sort==AT_CHARIOT_ARCHER||e.Sort==AT_STONE_THROWER)score+=5;
                if(score>bestScore){
                    bestScore=score;
                    bestSN=e.SN;
                }
            }
            enemyAttackingPriestSN=bestSN;
        };
        getThreatEnemySN();
        auto defendAttack=[&](tagArmy& a){
            int bestSN=-1;
            if(enemyAttackingPriestSN!=-1)bestSN=enemyAttackingPriestSN;
            if(bestSN!=-1)HumanAction(a.SN,bestSN);
        };
        auto priestEscape=[&](){
            double bestScore=-1e9;
            int bestDR=-1;
            int bestUR=-1;
            int startDR=max(priestBlockDR-8,0);
            int startUR=max(priestBlockUR-8,0);
            int endDR=min(priestBlockDR+8,100);
            int endUR=min(priestBlockUR+8,100);
            for(int dr=startDR;dr<endDR;dr++){
                for(int ur=startUR;ur<endUR;ur++){
                    if(!reachable[dr][ur])continue;
                    double score=0;
                    for(tagArmy& e:info.armies){
                        if(calDistance(e.DR,e.UR,priestDR,priestUR)/BLOCKSIDELENGTH>18)continue;
                        double d=calDistance(dr*BLOCKSIDELENGTH,ur*BLOCKSIDELENGTH,e.DR,e.UR)/BLOCKSIDELENGTH;
                        score+=d;
                    }
                    if(score>bestScore){
                        bestScore=score;
                        bestDR=dr;
                        bestUR=ur;
                    }
                }
            }
            if(bestDR==-1||bestUR==-1)return;
            HumanMove(priestSN,bestDR*BLOCKSIDELENGTH,bestUR*BLOCKSIDELENGTH);
        };
        auto priestConvert=[&](){
            if(priestWorkObejctSN!=-1)return;
            if(canConvert&&info.enemy_armies.size()!=0){
                double bestD=1e9;
                int bestSN=-1;
                for(tagArmy& e:info.enemy_armies){
                    double d=calDistance(priestDR,priestUR,e.DR,e.UR);
                    if(d<bestD){
                        bestD=d;
                        bestSN=e.SN;
                    }
                }
                if(bestSN!=-1)HumanAction(priestSN,bestSN);
                return;
            }
            bool danger=false;
            for(tagArmy& e:info.enemy_armies){
                double d=calDistance(e.DR,e.UR,priestDR,priestUR)/BLOCKSIDELENGTH;
                if(d<18)danger=true;
            }
            if(danger)priestEscape();
            else if(priestBlockDR!=baseBlockDR-2||priestBlockUR!=baseBlockUR-2)HumanMove(priestSN,(baseBlockDR-2)*BLOCKSIDELENGTH,(baseBlockUR-2)*BLOCKSIDELENGTH);
        };
        auto towerAutoAttack = [&](tagBuilding& b) {
            int targetSN=-1;
            int blood = 1e9;
            for (tagArmy& e : info.enemy_armies) {
                if (!towerTargetInRange(b.BlockDR, b.BlockUR, e.BlockDR, e.BlockUR))continue;
                if (e.Blood < blood) {
                    blood = e.Blood;
                    targetSN = e.SN;
                }
            }
            if (targetSN != -1) {
                HumanAction(b.SN, targetSN);
            }
        };
        if(!allOut_started&&!needExploration){
            for(tagArmy& a:info.armies){
                if(a.WorkObjectSN!=-1)continue;
                if(a.Sort==AT_PRIEST)continue;
                if((a.SN+timer)%38!=0)continue;
                defendAttack(a);
            }
            for(tagBuilding& b:info.buildings){
                if(b.Type!=BUILDING_ARROWTOWER)continue;
                if(b.Project!=-1)continue;
                towerAutoAttack(b);
            }
            if(timer%19==0)priestConvert();
        }
    }
    //--------------------祭司探图--------------------
    if (1) {
        // 找大本营
        if (!baseFound) {
            for (tagBuilding& b : info.buildings) {
                if (b.Type == BUILDING_CENTER) {
                    baseSN = b.SN;
                    baseBlockDR = b.BlockDR;
                    baseBlockUR = b.BlockUR;
                    baseFound = true;
                }
            }
        }

        if (needExploration&&baseFound) {
            if(coolDown==0){
                double bestScore=-1e9;
                double bestDR=-1;
                double bestUR=-1;
                for(auto f:frontier){
                    int dr=f.first;
                    int ur=f.second;
                    double score=0;
                    double dToBase=calDistance(dr*BLOCKSIDELENGTH,ur*BLOCKSIDELENGTH,baseBlockDR*BLOCKSIDELENGTH,baseBlockUR*BLOCKSIDELENGTH)/BLOCKSIDELENGTH;
                    double dToSelf=calDistance(dr*BLOCKSIDELENGTH,ur*BLOCKSIDELENGTH,priestDR,priestUR)/BLOCKSIDELENGTH;
                    score-=dToBase;
                    score-=1.5*dToSelf;
                    if(dToBase>70)score-=10000;
                    if(score>bestScore){
                        bestScore=score;
                        bestDR=dr;
                        bestUR=ur;
                    }
                }
                if(bestScore<=-10000){
                    DebugText("太远了,回家");
                    HumanMove(priestSN,(baseBlockDR-2)*BLOCKSIDELENGTH,(baseBlockUR-2)*BLOCKSIDELENGTH);
                    needExploration=false;
                }
                else if(bestDR!=-1&&bestUR!=-1){
                    HumanMove(priestSN,bestDR*BLOCKSIDELENGTH,bestUR*BLOCKSIDELENGTH);
                    coolDown=50;
                }
            }
            // 探图结束回基地
            if(timer>5600){
                needExploration=false;
                HumanMove(priestSN,(baseBlockDR-2)*BLOCKSIDELENGTH,(baseBlockUR-2)*BLOCKSIDELENGTH);
            }
            // 移动命令冷却
            if (coolDown > 0) {
                coolDown--;
            }
        }
    }
    //--------------------发动总攻--------------------
    if (1) {
        bool hasMelee=false;
        int meleeCnt=0;
        bool hasStoneThrower=false;
        int stoneThrowerSN=-1;
        if(1){
            for(tagArmy& e:info.enemy_armies){
                if(!isRanged(e.Sort)){
                    hasMelee=true;
                    meleeCnt++;
                }
            }
            for(tagArmy& e:info.enemy_armies){
                if(e.Sort==AT_STONE_THROWER){
                    hasStoneThrower=true;
                    if(!visableBlock[e.BlockDR][e.BlockUR])continue;
                    if(e.WorkObjectSN!=-1)stoneThrowerSN=e.SN;
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
                dest={baseBlockDR-2,baseBlockUR-2};
                DebugText("俺找不到边界,只能往家走了...");
            }
            HumanMove(army.SN,dest.first*BLOCKSIDELENGTH,dest.second*BLOCKSIDELENGTH);
        };
        auto allOut_escape=[&](tagArmy& army){
            int searchRange=8;
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
                    if(calDistance(dr*BLOCKSIDELENGTH,ur*BLOCKSIDELENGTH,army.DR,army.UR)/BLOCKSIDELENGTH>searchRange)continue;
                    double score = 0;
                    if (curMap[dr][ur]!=0)continue;
                    for (tagArmy& enemy : info.enemy_armies) {
                        double dToEnemy = calDistance(army.DR, army.UR, enemy.DR, enemy.UR);
                        if (dToEnemy > searchRange * BLOCKSIDELENGTH)continue;
                        score += abs(dr - enemy.BlockDR) + abs(ur - enemy.BlockUR);
                    }
                    score+=abs(dr-allOut_campDR)+abs(ur-allOut_campUR);
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
                    if(calDistance(army.DR,army.UR,e.DR,e.UR)<=11*BLOCKSIDELENGTH){
                        valid=true;
                        //if(army.NowState==HUMAN_STATE_IDLE)
                        if((army.Sort==AT_CHARIOT_ARCHER||army.Sort==AT_HOPLITE)&&timer%38!=0)break;
                        HumanAction(army.SN,e.SN);
                    }
                }
            }
            bool danger=false;
            if(isRanged(army.Sort)){
                int awarenessRange=5;
                if(!hasMelee&&stoneThrowerSN==-1)awarenessRange=2;
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
            /////////////////////////////////////
            if((locked==-1||!valid)&&!danger){
                if(hasMelee||(!hasMelee&&stoneThrowerSN==-1)){
                    double bestD=1e9;
                    int bestSN=-1;
                    for(tagArmy& e:info.enemy_armies){
                        if(!visableBlock[e.BlockDR][e.BlockUR])continue;
                        double d=calDistance(army.DR,army.UR,e.DR,e.UR);
                        if(d<bestD){
                            bestD=d;
                            bestSN=e.SN;
                        }
                    }
                    allOut_rangedLock[army.SN]=bestSN;
                    //if(army.NowState!=HUMAN_STATE_WALKING&&bestSN!=-1)HumanAction(army.SN,bestSN);
                    if(bestSN!=-1)HumanAction(army.SN,bestSN);
                }else{
                    allOut_rangedLock[army.SN]=stoneThrowerSN;
                    //if(army.NowState!=HUMAN_STATE_WALKING&&stoneThrowerSN!=-1)HumanAction(army.SN,stoneThrowerSN);
                    if(stoneThrowerSN!=-1)HumanAction(army.SN,stoneThrowerSN);
                }
            }
            /////////////////////////////////////
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
                        if(a.Sort==AT_PRIEST&&(meleeCnt>0||priestState==HUMAN_STATE_ATTACKING||!canConvert||info.enemy_armies.size()>5))continue;
                        //战车弓兵的攻击间隔是1.5秒,即37.5帧
                        //if((a.Sort==AT_CHARIOT_ARCHER||a.Sort==AT_HOPLITE)&&timer%38!=0)continue;
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
        if (mark % 24 <= 1){berryFarmers.push_back(farmerSN);freeFarmers.push_back(farmerSN);foodFarmers.push_back(farmerSN);}
        else if (mark % 24 <= 4){woodFarmers.push_back(farmerSN);freeFarmers.push_back(farmerSN);}
        else if (mark % 24 <= 6){buildingFarmers.push_back(farmerSN);freeFarmers.push_back(farmerSN);}
        else if (mark % 24 <= 12){hunterFarmers.push_back(farmerSN);freeFarmers.push_back(farmerSN);foodFarmers.push_back(farmerSN);}
        else if (mark % 24 <= 20){woodFarmers.push_back(farmerSN);freeFarmers.push_back(farmerSN);}
        else if (mark % 24 <= 22)farmFarmers.push_back(farmerSN);
        else {stoneFarmers.push_back(farmerSN);freeFarmers.push_back(farmerSN);}
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
                        score += (20 - dToGranary);
                        break;
                    }
                    score+=0.5*calDistance(dr*BLOCKSIDELENGTH,ur*BLOCKSIDELENGTH,baseBlockDR*BLOCKSIDELENGTH,baseBlockUR*BLOCKSIDELENGTH)/BLOCKSIDELENGTH;
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
            if(!hasAnimal&&!hasBush)farmers=&foodFarmers;
        }else if(type == BUILDING_STOCK){
            farmers = &hunterFarmers;
            if(timer>18000)farmers=&freeFarmers;
        }else if(type==BUILDING_GRANARY){
            farmers=&berryFarmers;
            if(!hasAnimal)farmers=&foodFarmers;
            //if(!hasAnimal&&timer>18000)farmers=&freeFarmers;
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
    auto specializedBuild=[&](tagFarmer& f,int type,int rDR,int rUR){
        double bestScore=-1e9;
        double bestDR=-1;
        double bestUR=-1;
        int startDR=max(0,rDR-10);
        int startUR=max(0,rUR-10);
        int endDR=min(100-4,rDR+6);
        int endUR=min(100-4,rUR+6);
        for(int dr=startDR;dr<endDR;dr++){
            for(int ur=startUR;ur<endUR;ur++){
                bool canBuild=true;
                for(int i=0;i<4&&canBuild;i++){
                    for(int j=0;j<4&&canBuild;j++){
                        if(curMap[dr+i][ur+j]!=0){
                            canBuild=false;
                        }
                    }
                }
                if(!canBuild)continue;
                double score=0;
                double d=calDistance(dr*BLOCKSIDELENGTH,ur*BLOCKSIDELENGTH,rDR*BLOCKSIDELENGTH,rUR*BLOCKSIDELENGTH)/BLOCKSIDELENGTH;
                score-=d;
                if(score>bestScore){
                    bestScore=score;
                    bestDR=dr;
                    bestUR=ur;
                }
            }
        }
        if(bestDR==-1||bestUR==-1)return;
        DebugText("有合法位置");
        HumanBuild(f.SN,type,bestDR,bestUR);
        if(type==BUILDING_STOCK)bugdetWood+=BUILD_STOCK_WOOD;
        else if(type==BUILDING_GRANARY)bugdetWood+=BUILD_GRANARY_WOOD;
    };
    auto hunt=[&](){
        double bestScore=-1e9;
        int bestSN=-1;
        int itsDR=-1;
        int itsUR=-1;
        bool needNewStock=true;
        bool needHelp=false;
        int needHelpSN=-1;
        vector<int>*farmers;
        if(timer>18000)farmers=&freeFarmers;
        else farmers=&hunterFarmers;

        for(tagResource& a:info.resources){
            if(a.Type!=RESOURCE_ELEPHANT&&a.Type!=RESOURCE_GAZELLE)continue;
            if(timer<18000&&a.Type==RESOURCE_ELEPHANT)continue;
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
                if(timer>18000){
                    if(a.Type==RESOURCE_ELEPHANT){
                        score-=(d+5);
                        if(busyObject.count(a.SN)){
                            score+=10;
                            if(busyObject[a.SN]>5)score-=30;
                        }
                    }
                    if(a.Type==RESOURCE_GAZELLE){
                        score-=d;
                        if(busyObject.count(a.SN)&&busyObject[a.SN]>1)score-=15;
                    }
                }
                else{
                    if(a.Type==RESOURCE_ELEPHANT){
                        score-=(d+5);
                        if(hunterFarmers.size()<5)score-=100;
                        if(busyObject.count(a.SN))score+=10;
                    }
                    if(a.Type==RESOURCE_GAZELLE){
                        score-=d;
                        if(busyObject.count(a.SN)&&busyObject[a.SN]>1)score-=15;
                    }
                }
                if(score>bestScore){
                    bestScore=score;
                    bestSN=a.SN;
                    itsDR=a.BlockDR;
                    itsUR=a.BlockUR;
                }
            }
        }
        if(bestSN==-1||itsDR==-1||itsUR==-1)return;
        for(tagBuilding& b:info.buildings){
            if(b.Type!=BUILDING_STOCK&&b.Type!=BUILDING_CENTER)continue;
            double d=calDistance(itsDR*BLOCKSIDELENGTH,itsUR*BLOCKSIDELENGTH,b.BlockDR*BLOCKSIDELENGTH,b.BlockUR*BLOCKSIDELENGTH)/BLOCKSIDELENGTH;
            if(d<15){
                needNewStock=false;
                if(b.Percent<100){
                    needHelp=true;
                    needHelpSN=b.SN;
                }
                break;
            }
        }
        for(int sn:*farmers){
            for(tagFarmer& f:info.farmers){
                if(f.SN!=sn)continue;
                if(f.NowState!=HUMAN_STATE_IDLE)break;
                if(needNewStock&&info.Wood-bugdetWood>=BUILD_STOCK_WOOD){
                    specializedBuild(f,BUILDING_STOCK,itsDR,itsUR);
                }
                else if(needHelp){
                    HumanAction(sn,needHelpSN);
                }
                else{
                    HumanAction(sn,bestSN);
                }
                return;
            }
        }
    };
    auto gatherBerry=[&](){
        int bestSN=-1;
        double bestD=1e9;
        int itsDR=-1;
        int itsUR=-1;
        bool needNewGranary=true;
        bool needHelp=false;
        int needHelpSN=-1;
        for(tagResource& r:info.resources){
            if(r.Type!=RESOURCE_BUSH)continue;
            if(calDistance(r.DR,r.UR,allOut_campDR*BLOCKSIDELENGTH,allOut_campUR*BLOCKSIDELENGTH)/BLOCKSIDELENGTH<25)continue;
            if(busyObject.count(r.SN)&&busyObject[r.SN]>3)continue;
            for(tagBuilding& b:info.buildings){
                if(b.Type!=BUILDING_GRANARY&&b.Type!=BUILDING_CENTER)continue;
                double d=calDistance(r.DR,r.UR,b.BlockDR*BLOCKSIDELENGTH,b.BlockUR*BLOCKSIDELENGTH)/BLOCKSIDELENGTH;
                if(d<bestD){
                    bestD=d;
                    bestSN=r.SN;
                    itsDR=r.BlockDR;
                    itsUR=r.BlockUR;
                }
            }
        }
        if(bestSN==-1||itsDR==-1||itsUR==-1)return;
        for(tagBuilding& b:info.buildings){
            if(b.Type!=BUILDING_GRANARY&&b.Type!=BUILDING_CENTER)continue;
            double d=calDistance(itsDR*BLOCKSIDELENGTH,itsUR*BLOCKSIDELENGTH,b.BlockDR*BLOCKSIDELENGTH,b.BlockUR*BLOCKSIDELENGTH)/BLOCKSIDELENGTH;
            if(d<15){
                needNewGranary=false;
                if(b.Percent<100){
                    needHelp=true;
                    needHelpSN=b.SN;
                }
                break;
            }
        }
        vector<int>*farmers=&berryFarmers;
        if(!hasAnimal)farmers=&foodFarmers;
        if(timer>18000)farmers=&freeFarmers;
        for(int sn:*farmers){
            for(tagFarmer& f:info.farmers){
                if(f.SN!=sn)continue;
                if(f.NowState!=HUMAN_STATE_IDLE)break;
                if(needNewGranary&&info.Wood-bugdetWood>=BUILD_GRANARY_WOOD){
                    specializedBuild(f,BUILDING_GRANARY,itsDR,itsUR);
                }else if(needHelp){
                    HumanAction(sn,needHelpSN);
                }else{
                    HumanAction(sn,bestSN);
                }
                return;
            }
        }
    };
    if(timer>18000){
        //if(timer>30000&&info.enemy_armies.size()==0){
        if(0){
            if(info.Gold*6<info.Meat*4||(!hasAnimal&&!hasBush)){
                assignTask(freeFarmers,RESOURCE_GOLD);
            }else{
                if(hasAnimal)hunt();
                else if(hasBush){
                    gatherBerry();
                }
            }
        }
        else {
            if(info.Wood*4<info.Meat*7||info.Wood<300||(!hasAnimal&&!hasBush)){
                assignTask(freeFarmers,RESOURCE_TREE);
            }else{
                if(hasAnimal)hunt();
                else if(hasBush){
                    gatherBerry();
                }
            }
        }
    }
    else {
        if(hasAnimal)hunt();
        else if(hasBush){
            assignTask(hunterFarmers,RESOURCE_BUSH);
        }
    }
    gatherBerry();
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
        if (info.Wood-bugdetWood >= BUILD_FARM_WOOD && marketCnt&&WOODUnlocked){build(BUILDING_FARM);bugdetWood+=BUILD_FARM_WOOD;}
        if(timer>15000&&rangeCnt1<4&&info.Wood-bugdetWood >= BUILD_RANGE_WOOD){build(BUILDING_RANGE);bugdetWood+=BUILD_RANGE_WOOD;}
        if(timer>13500&&homeCnt1<12 && info.Wood-bugdetWood >= BUILD_HOUSE_WOOD){build(BUILDING_HOME);bugdetWood+=BUILD_HOUSE_WOOD;}
        if(timer>30000&&!collageCnt1 && info.Wood-bugdetWood >= BUILD_COLLAGE_WOOD){build(BUILDING_COLLAGE);bugdetWood+=BUILD_COLLAGE_WOOD;}
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
        if (info.Meat-bugdetMeat >= 800&&info.civilizationStage!=CIVILIZATION_BRONZEAGE) {
            BuildingAction(baseSN, BUILDING_CENTER_UPGRADE);
            bugdetMeat+=800;
        }
        //生产村民
        if (info.farmers.size() < 24 && info.farmers.size() + info.armies.size() < info.Human_MaxNum&&timer<35000) {
            if(info.farmers.size()<18||WOODUnlocked){
                for (tagBuilding& building : info.buildings) {
                    if (building.Type != BUILDING_CENTER)continue;
                    if(building.Project==BUILDING_CENTER_CREATEFARMER)continue;
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
        // for(tagBuilding &building:info.buildings){
        //     if(building.Type!=BUILDING_COLLAGE)continue;
        //     if(info.Gold-bugdetGold>=BUILDING_COLLAGE_CREATE_HOPLITE_GOLD&&info.Meat-bugdetMeat>=BUILDING_COLLAGE_CREATE_HOPLITE_FOOD){
        //         BuildingAction(building.SN,BUILDING_COLLAGE_CREATE_HOPLITE);
        //         bugdetGold+=BUILDING_COLLAGE_CREATE_HOPLITE_GOLD;
        //         bugdetMeat+=BUILDING_COLLAGE_CREATE_HOPLITE_FOOD;
        //     }
        // }
        if (wheelUnlocked&&craftUnlocked) {
            //生产战车弓兵
            if (info.farmers.size() + info.armies.size() < info.Human_MaxNum) {
                for (tagBuilding& building : info.buildings) {
                    if (building.Type != BUILDING_RANGE)continue;
                    if(building.Project==BUILDING_RANGE_CREATE_CHARIOT_ARCHER)continue;
                    if (info.Meat-bugdetMeat >= BUILDING_RANGE_CREATE_CHARIOT_ARCHER_FOOD && info.Wood-bugdetWood >= BUILDING_RANGE_CREATE_CHARIOT_ARCHER_WOOD
                    &&info.Wood-bugdetWood-BUILDING_RANGE_CREATE_CHARIOT_ARCHER_WOOD>=50) {
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
        if(WOODResearchTimer==1005&&!WOODUnlocked){
            WOODUnlocked=true;
            WOODResearching=false;
            DebugText("木材加工研发完成");
        }
        if(!WOODResearching&&!WOODUnlocked){
            for(tagBuilding& b:info.buildings){
                if(b.Type!=BUILDING_MARKET)continue;
                if(b.Percent<100)continue;
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
        if(craftResearching){
            craftResearchTimer++;
        }
        if(craftResearchTimer==1005&&!craftUnlocked){
            craftUnlocked=true;
            craftResearching=false;
            DebugText("工艺研发完成");
        }
        if(info.civilizationStage == CIVILIZATION_BRONZEAGE&&!craftResearching&&!craftUnlocked&&WOODUnlocked&&wheelUnlocked){
            for(tagBuilding& b:info.buildings){
                if(b.Type!=BUILDING_MARKET)continue;
                if(info.Wood-bugdetWood>=BUILDING_MARKET_CRAFT_UPGRADE_WOOD&&
                    info.Meat-bugdetMeat>=BUILDING_MARKET_CRAFT_UPGRADE_FOOD){
                    BuildingAction(b.SN,BUILDING_MARKET_WOOD_UPGRADE);
                    bugdetWood+=BUILDING_MARKET_CRAFT_UPGRADE_WOOD;
                    bugdetMeat+=BUILDING_MARKET_CRAFT_UPGRADE_FOOD;
                    craftResearching=true;
                    DebugText("开始研发工艺");
                }
                break;
            }
        } 
        if (wheelResearching) {
            wheelResearchTimer++;
        }
        if (wheelResearchTimer == 1005 && !wheelUnlocked) {
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