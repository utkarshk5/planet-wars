#include <iostream>
#include "PlanetWars.h"
#include <map>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sys/time.h>
using namespace std;

bool operator < (const Planet& p1, const Planet& p2){
	return p1.PlanetID() < p2.PlanetID();
}

bool operator < (const Fleet& f1, const Fleet& f2){
	return f1.TurnsRemaining() < f2.TurnsRemaining();
}


ofstream out("orders.txt", ios::app);

bool debug= false;
map<Planet, int> frontierType;
map<Planet, int> botsAvailable;
map<Planet, bool> goingToDie;
map<Planet, vector<Fleet> > fleetsIncoming;

//vector<Fleet> myFleets;
vector<Fleet> myActualFleets;
vector<Fleet> enemyFleets;
//vector<Fleet> allFleets;
vector<Planet> allPlanets;
vector<Planet> myPlanets;
vector<Planet> notMyPlanets;

const PlanetWars* pw;
const int myOnly = 1;
const int enemyOnly = 2;
const int neutralOnly = 3;
const int notMyOnly = 4;
const int allOnly = 5;
const int frontierPlanet = 1;
const int interiorPlanet = 2;
const int fleetThreshold = 10;

int closestPlanet(Planet p, int type){
	int id = -1;
	int dist = 1e9;
	for(auto other: allPlanets){
		if(p.PlanetID() == other.PlanetID()) continue;
		if(type == myOnly && other.Owner()!=1) continue;
		if(type == enemyOnly && other.Owner() <= 1) continue;
		if(type == neutralOnly && other.Owner() != 0) continue;
		if(type == notMyOnly && other.Owner() == 1) continue;
		if(pw->Distance(p.PlanetID(), other.PlanetID()) < dist){
			dist = pw->Distance(p.PlanetID(), other.PlanetID());
			id = other.PlanetID();
		}
	}
	return id;
}
			
Planet getPlanetFromID(int id){
	for(auto p: allPlanets)
		if(p.PlanetID() == id)
			return p;
}

void initializeTurn(){
	frontierType.clear();
	botsAvailable.clear();
	fleetsIncoming.clear();
	goingToDie.clear();
	//allFleets = pw->Fleets();
	//myFleets = pw->MyFleets();
	enemyFleets = pw->EnemyFleets();
	allPlanets = pw->Planets();
	myPlanets = pw->MyPlanets();
	notMyPlanets = pw->NotMyPlanets();
	
	// my actual fleets
	vector<Fleet> temp;
	
	for(auto fleet: myActualFleets){
		fleet.turns_remaining_--;
		if(fleet.TurnsRemaining() > 0){
			temp.push_back(fleet);
		}
	}
	myActualFleets=temp;
	
	
	// frontier tagging for own planets
	for(auto planet: myPlanets){
		int closestEnemy = closestPlanet(planet, enemyOnly);
		if(closestEnemy == -1) break;
		Planet closestEnemyPlanet = getPlanetFromID(closestEnemy);
		int closestOfEnemy = closestPlanet(closestEnemyPlanet, myOnly);
		if(closestOfEnemy == planet.PlanetID()){
			frontierType[planet] = frontierPlanet;
			continue;
		}
		frontierType[planet] = interiorPlanet;
		
	}
	
	// may want to add tagging for enemy planets (frontier/not frontier)
	for(auto planet: pw->EnemyPlanets()){
		int closestEnemy = closestPlanet(planet, myOnly);
		if(closestEnemy == -1) break;
		Planet closestEnemyPlanet = getPlanetFromID(closestEnemy);
		int closestOfEnemy = closestPlanet(closestEnemyPlanet, enemyOnly);
		if(closestOfEnemy == planet.PlanetID()){
			frontierType[planet] = frontierPlanet;
			continue;
		}
		frontierType[planet] = interiorPlanet;
		
	}


	// fleets incoming
	for(auto fleet: enemyFleets){
		Planet dest = getPlanetFromID(fleet.DestinationPlanet());
		fleetsIncoming[dest].push_back(fleet);
	}
	for(auto fleet: myActualFleets){
		Planet dest = getPlanetFromID(fleet.DestinationPlanet());
		fleetsIncoming[dest].push_back(fleet);
	}
	
	for(auto p: allPlanets){
		sort(fleetsIncoming[p].begin(), fleetsIncoming[p].end());
	}

	// populating botsAvailable
	for(auto p: allPlanets){
		botsAvailable[p] = p.NumShips();
		int netShips = 0;
		int minAvailable = p.NumShips();
		for(auto q: fleetsIncoming[p]){
			int turns = q.TurnsRemaining();
			int thresholdToUse = fleetThreshold;
			
			if(p.Owner() == 1 && frontierType[p]==frontierPlanet){
				int closest = 1e9;
				for(auto myOther: myPlanets){
					if(frontierType[myOther] == interiorPlanet){
						closest = min(closest, pw->Distance(myOther.PlanetID(), p.PlanetID()));
					}
				}
				if(closest!=1e9) thresholdToUse = min(thresholdToUse, closest + 2);
			}
			
			
			if(turns > thresholdToUse) continue;
			int ships = q.NumShips();
			int owner = q.Owner();
			if(p.Owner()==1){
				if(owner==1) netShips+=ships;
				else netShips -= ships;
			}
			if(p.Owner() > 1){
				if(owner==1) netShips -=ships;
				else netShips += ships;
			}
			if(p.Owner() == 0)
				netShips -= ships;
			int growthShips = (turns-1)*p.GrowthRate()*(p.Owner()!=0);
			int total = p.NumShips() + netShips + growthShips;
			int temp = 0;
			if(total > p.NumShips()) temp = p.NumShips()-1;
			else temp = total-1;
			if(total <= 0) minAvailable = total;
			else minAvailable = min(minAvailable, temp);
		}
		botsAvailable[p]=minAvailable;
		if(p.Owner()==1) botsAvailable[p] = botsAvailable[p] - p.GrowthRate();
		if(p.Owner()==0) botsAvailable[p] = botsAvailable[p] +1;
		if(p.Owner()>1) botsAvailable[p] = botsAvailable[p] + p.GrowthRate();  
		if(botsAvailable[p] <= 2*p.GrowthRate())
			goingToDie[p]=true;
		else
			goingToDie[p]=false;
	}
	
}
	
int myIssueOrder(Planet source, Planet dest, int num){
	int ships = source.NumShips()-1;
	int available = botsAvailable[source];
	if(num >= ships) num = ships;
	if(num < 0) num = 0;
	int d = pw->Distance(source.PlanetID(), dest.PlanetID());
	//if(debug) out<<source.PlanetID()<<" "<<dest.PlanetID()<<" "<<source.NumShips()<<" "<<num<<endl;
	Fleet f(1, num, source.PlanetID(), dest.PlanetID(), d, d);
	if(num > 0)
		myActualFleets.push_back(f);
	pw->IssueOrder(source.PlanetID(), dest.PlanetID(), num);
	botsAvailable[source]-=num;
	botsAvailable[dest]+=num;
	return num;
}
	
// smaller the better
float sourceTargetScore(Planet source, Planet target){
	
	/*
		choose one "not my planet" based on a linear combination of botsAvailable, distance, growth rate, owner, "sniping factors"
		attack with out bots depending on growth rate, and "closeness" to enemies after capturing
	*/
	
	float availability = botsAvailable[target]/50.0;
	float growthRate = target.GrowthRate()/5.0;
	float isNeutral = target.Owner()==0;
	float distance = pw->Distance(source.PlanetID(), target.PlanetID()) / 10.0;
	
	// sniping factors
	if(target.Owner() == 0 && fleetsIncoming.size() != 0){
		int enemyShips = 0;
		for(auto fleet: fleetsIncoming[target]){
			if(fleet.TurnsRemaining() < pw->Distance(source.PlanetID(), target.PlanetID())) continue;
			enemyShips += fleet.NumShips();
		}
		if(enemyShips >= target.NumShips()) return 10000;
	}
	// if(target.Owner() > 1 && frontierType[target]==interiorPlanet) return 3*availability + -2*growthRate + 1*isNeutral + 6*distance - 2; 
	return 3*availability + -2*growthRate + 1*isNeutral + 6*distance;

}

float sourceSourceScore(Planet source, Planet other){
	float distance = pw->Distance(source.PlanetID(), other.PlanetID())/10.0;
	float ships = botsAvailable[source]/50.0;
	
	return -ships*3 + distance*4;
}

bool supportDyingInteriors(){
	
	int minAvail = 1e9;
	int best = -1;
	
	for(auto supported: myPlanets){		
		if(frontierType[supported]!=interiorPlanet) continue;
		if(botsAvailable[supported] >= 1) continue;
		
		if(minAvail < botsAvailable[supported]){
			minAvail = botsAvailable[supported];
			best = supported.PlanetID();
		}
	}
	if(best == -1) return false;
	
	Planet supported = getPlanetFromID(best);
	
	map<float, int> supporters;
	int index = 0;
	for(auto p : myPlanets){
		if(frontierType[p]!=interiorPlanet) continue;
		float score = sourceSourceScore(p, supported) + (0.001)*(index++);
		supporters[score]=p.PlanetID();
	}
	for(auto it = supporters.begin(); it!=supporters.end(); it++){
		if(botsAvailable[supported] > 0) break;
		Planet supporter = getPlanetFromID(it->second);
		
		myIssueOrder(supporter, supported, -botsAvailable[supported] + 5);
	}
	
	if(botsAvailable[supported] <= 0) return false;	
		
	return true;
}

bool supportDyingFrontiers(){
	
	int minAvail = 1e9;
	int best = -1;
	
	for(auto supported: myPlanets){		
		if(frontierType[supported]!=frontierPlanet) continue;
		if(botsAvailable[supported] >= 1) continue;
		
		if(minAvail < botsAvailable[supported]){
			minAvail = botsAvailable[supported];
			best = supported.PlanetID();
		}
	}
	if(best == -1) return false;
	
	Planet supported = getPlanetFromID(best);
	
	map<float, int> supporters;
	int index = 0;
	for(auto p : myPlanets){
		if(frontierType[p]!=interiorPlanet) continue;
		float score = sourceSourceScore(p, supported) + (0.001)*(index++);
		supporters[score]=p.PlanetID();
	}
	for(auto it = supporters.begin(); it!=supporters.end(); it++){
		if(botsAvailable[supported] > 0) break;
		Planet supporter = getPlanetFromID(it->second);
		
		myIssueOrder(supporter, supported, -botsAvailable[supported] + 5);
	}
	
	if(botsAvailable[supported] <= 0) return false;	
	
	return true;
}

void supportLivingFrontiers(){
	
	for(auto p : myPlanets){
		if(frontierType[p]!=interiorPlanet) continue;
		
		float score = 1e9;
		int best = -1;
		for(auto pp : myPlanets){
			if(frontierType[pp]!=frontierPlanet) continue;
			float dist = pw->Distance(pp.PlanetID(), p.PlanetID())/10.0;
			float num = botsAvailable[pp]/50.0;
			float temp = 3*dist + 2*num;
			if(score > temp){
				score = temp;
				best = pp.PlanetID();
			}
		}
		if(best == -1) continue;
		
		myIssueOrder(p, getPlanetFromID(best), 0.75*botsAvailable[p]);
	}
			
}

Planet* firstTurnPlanet;
float firstTurnDistance;

float firstTurnHeuristic(Planet p1){
	float ships= p1.NumShips();
	vector<Fleet> eFleets = pw->EnemyFleets();
	for(auto it = eFleets.begin(); it!=eFleets.end(); it++){
		if(it->DestinationPlanet() != p1.PlanetID()) continue;
		ships -= it->NumShips();
	}
	
	float dist= pw->Distance(p1.PlanetID(), firstTurnPlanet->PlanetID());
	float growthRate = p1.GrowthRate();
	float a = 0.2, b = -1.5;
	if(dist > firstTurnDistance) return 100000;
	return a*ships + b*growthRate;
}

bool myComp(const Planet& p1, const Planet& p2){
	return (firstTurnHeuristic(p1) < firstTurnHeuristic(p2));
}

int turn = 0;
//clock_t beginOfTurn;
clock_t endOfLastTurn;


//In microseconds
long int gTimeOut = 0;
long int gStartTime = 0;
long int SECONDS_PER_DAY = 24*60*60;


void SetTimeOut(double seconds) {
	timeval startTime;
	gettimeofday(&startTime, NULL);

	gStartTime = (startTime.tv_sec % SECONDS_PER_DAY)* 1000 + startTime.tv_usec / 1000;
	gTimeOut = static_cast<long int>(seconds * 1000.0);
}

bool HasTimedOut() {
	timeval currentTime;
	gettimeofday(&currentTime, NULL);

	long int currentMilliseconds = (currentTime.tv_sec % SECONDS_PER_DAY) * 1000 + currentTime.tv_usec / 1000;

	return ((currentMilliseconds - gStartTime) > gTimeOut);
}
	
void DoTurn(const PlanetWars& pwi) {
	
	
	turn++;
	if(turn==1){
		//if(debug) out<<"************** NEW GAME **************"<<endl;
	}
	//if(debug) out<<"************ Turn "<<turn<<"************"<<endl;
	pw = &pwi;
	
	/* reinforce, neutral sniping, normal attack
	
	sort frontier planets based on "attacking" potential
	
	for each of our frontier planets:
			choose one "not my planet" based on a linear combination of botsAvailable, distance, growth rate, owner, "sniping factors"
			attack with our bots depending on growth rate, and "closeness" to enemies after capturing
	*/

	initializeTurn();

	if(turn==1){
		vector<Planet> neutrals = pw->NeutralPlanets();
		firstTurnPlanet = &myPlanets[0];
		firstTurnDistance = 7;
		sort(neutrals.begin(), neutrals.end(), &myComp);
		int ships = myPlanets[0].NumShips();
		
		for(auto it = neutrals.begin(); it!=neutrals.end(); it++){
			int extra = 5;
			int toSend = it->NumShips() + extra;			
			if(ships<extra) return;
			ships -= toSend;
			if(ships<=extra) return;
			myIssueOrder(myPlanets[0], *it, toSend);
		}
		endOfLastTurn = clock();
		return;
	}
	
	{
		map<float, int> attackPotential;
		
		int index=0;
		
		for(auto attacker: myPlanets){
			if(frontierType[attacker]!=frontierPlanet) continue;
			attackPotential[-botsAvailable[attacker]+(0.0001)*(index++)] = attacker.PlanetID();
		}
		
		for(auto it = attackPotential.begin(); it!=attackPotential.end(); it++){
			Planet attacker = getPlanetFromID(it->second);
			float bestScore = 100;
			
			Planet bestPlanet = attacker;
			
			for(auto otherPlanet : notMyPlanets){
				float score = sourceTargetScore(attacker, otherPlanet);
				if(botsAvailable[otherPlanet] < 0) continue;
				if(score > bestScore) continue;
				bestScore = score;
				bestPlanet = otherPlanet;
			}
			if(bestScore >= 100) continue;
			
			float otherGrowthRate = bestPlanet.GrowthRate();
			float otherCloseness = pw->Distance(bestPlanet.PlanetID(), closestPlanet(bestPlanet, enemyOnly));
			float myGrowthRate = attacker.GrowthRate();
			float myCloseness = pw->Distance(attacker.PlanetID(), closestPlanet(attacker, enemyOnly));
			
			float ratio = 1;
			if(otherGrowthRate > myGrowthRate) ratio = 0.9;
			else if(otherCloseness < myCloseness) ratio = 0.8;
			
			ratio = 0.5;
			
			int ships = botsAvailable[attacker]*ratio;
			
			if((ships+0.0)/bestPlanet.NumShips() < 0.3) continue;
			
			myIssueOrder(attacker, bestPlanet, ships);
			
			
		}
	}			
		
	/*
	for each of our support planets:
	 		choose one of our frontier planets based on botsAvailable, distance, "closeness" to enemies (is it worth keeping)
	 		support with all our available bots
	
	*/
	

	while(supportDyingInteriors()){
	}
	
	while(supportDyingFrontiers()){
	}
	
	supportLivingFrontiers();
	
	bool confusion = true;
	if(turn > 25) return;
	if(!confusion) return;
	for(auto p: myPlanets){
		if(HasTimedOut()){
			return;
		}
		
		for(auto pp : notMyPlanets){
			for(int i=0; i<200; i++){
				if(HasTimedOut()){
					return;
				}
				myIssueOrder(p, pp, 0);
			}
		}
	}
	
}


int main(int argc, char *argv[]) {
  std::string current_line;
  std::string map_data;
  while (true) {
	SetTimeOut(0.02);	
    int c = std::cin.get();
    current_line += (char)c;
    if (c == '\n') {
      if (current_line.length() >= 2 && current_line.substr(0, 2) == "go") {
        PlanetWars pw(map_data);
        map_data = "";
        DoTurn(pw);
	pw.FinishTurn();
      } else {
        map_data += current_line;
      }
      current_line = "";
    }
  }
  return 0;
}
