#include <ilcplex/ilocplex.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <chrono>
#include <random>

class DWTASolver {
private:
    // Problem dimensions
    int W; // Number of weapons
    int T; // Number of targets
    int K; // Number of assets
    int S; // Number of stages

    // Problem parameters
    std::vector<double> assetValues; // V
    std::vector<std::vector<double>> targetLethality; // Q
    std::vector<std::vector<std::vector<double>>> weaponLethality; // P
    std::vector<std::vector<std::vector<bool>>> engagementFeasibility; // F
    std::vector<int> maxWeaponUses; // N

    // Constraint saturation states
    std::vector<std::vector<bool>> C2F; // Weapon-stage constraints
    std::vector<std::vector<bool>> C3F; // Target-stage constraints
    std::vector<bool> C4F; // Weapon usage constraints

    // Decision variables
    std::vector<std::vector<std::vector<bool>>> decision; // X

    // Target survival probabilities
    std::vector<double> targetSurvivalProb;

    // Available weapons for each target at each stage
    std::vector<std::vector<std::vector<int>>> availableWeapons;

    // Random number generator
    std::mt19937 rng;

public:
    // Constructor
    DWTASolver(int numWeapons, int numTargets, int numAssets, int numStages)
        : W(numWeapons), T(numTargets), K(numAssets), S(numStages) {

        // Seed the random number generator
        unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        rng = std::mt19937(seed);

        // Initialize data structures
        assetValues.resize(K, 0.0);
        targetLethality.resize(T, std::vector<double>(K, 0.0));
        weaponLethality.resize(W, std::vector<std::vector<double>>(T, std::vector<double>(S, 0.0)));
        engagementFeasibility.resize(W, std::vector<std::vector<bool>>(T, std::vector<bool>(S, false)));
        maxWeaponUses.resize(W, 1);

        // Initialize constraint saturation states
        C2F.resize(W, std::vector<bool>(S, false));
        C3F.resize(T, std::vector<bool>(S, false));
        C4F.resize(W, false);

        // Initialize decision variables
        decision.resize(W, std::vector<std::vector<bool>>(T, std::vector<bool>(S, false)));

        // Initialize target survival probabilities
        targetSurvivalProb.resize(T, 1.0);

        // Initialize available weapons
        availableWeapons.resize(T, std::vector<std::vector<int>>(S));
    }

    // Set problem parameters
    void setAssetValues(const std::vector<double>& values) {
        if (values.size() == K) {
            assetValues = values;
        }
    }

    void setTargetLethality(const std::vector<std::vector<double>>& lethality) {
        if (lethality.size() == T && lethality[0].size() == K) {
            targetLethality = lethality;
        }
    }

    void setWeaponLethality(const std::vector<std::vector<std::vector<double>>>& lethality) {
        if (lethality.size() == W && lethality[0].size() == T && lethality[0][0].size() == S) {
            weaponLethality = lethality;
        }
    }

    void setEngagementFeasibility(const std::vector<std::vector<std::vector<bool>>>& feasibility) {
        if (feasibility.size() == W && feasibility[0].size() == T && feasibility[0][0].size() == S) {
            engagementFeasibility = feasibility;
        }
    }

    void setMaxWeaponUses(const std::vector<int>& maxUses) {
        if (maxUses.size() == W) {
            maxWeaponUses = maxUses;
        }
    }

    // Initialize available weapons based on engagement feasibility
    void initializeAvailableWeapons() {
        // For each target and stage, determine which weapons are available
        for (int j = 0; j < T; j++) {
            for (int t = 0; t < S; t++) {
                for (int i = 0; i < W; i++) {
                    if (engagementFeasibility[i][j][t]) {
                        availableWeapons[j][t].push_back(i);
                    }
                }
            }
        }
    }

    // Structure to represent a weapon-target-stage assignment pair with its VQP value
    struct AssignmentPair {
        int weaponIndex;
        int targetIndex;
        int stageIndex;
        double vqpValue;

        AssignmentPair(int w, int t, int s, double vqp)
            : weaponIndex(w), targetIndex(t), stageIndex(s), vqpValue(vqp) {}

        // For sorting in descending order of VQP value
        bool operator<(const AssignmentPair& other) const {
            return vqpValue > other.vqpValue;
        }
    };

    // Calculate VQP (Value of a Weapon-Target-Stage pair) value for an assignment pair
    double calculateVQP(int weaponIndex, int targetIndex, int stageIndex) {
        // Find which asset this target is aiming at
        int assetIndex = -1;
        double maxLethality = 0.0;

        for (int k = 0; k < K; k++) {
            if (targetLethality[targetIndex][k] > maxLethality) {
                maxLethality = targetLethality[targetIndex][k];
                assetIndex = k;
            }
        }

        if (assetIndex == -1) return 0.0; // Target doesn't threaten any asset

        // Calculate VQP = asset_value * target_lethality * weapon_lethality * target_survival_prob
        double assetValue = assetValues[assetIndex];
        double targetLethalityValue = targetLethality[targetIndex][assetIndex];
        double weaponLethalityValue = weaponLethality[weaponIndex][targetIndex][stageIndex];

        return assetValue * targetLethalityValue * weaponLethalityValue * targetSurvivalProb[targetIndex];
    }

    // Update target survival probability after an assignment
    void updateTargetSurvivalProb(int targetIndex) {
        targetSurvivalProb[targetIndex] = 1.0;

        for (int t = 0; t < S; t++) {
            for (int i = 0; i < W; i++) {
                if (decision[i][targetIndex][t]) {
                    targetSurvivalProb[targetIndex] *= (1.0 - weaponLethality[i][targetIndex][t]);
                }
            }
        }
    }

    // Main rule-based heuristic algorithm
    void solve() {
        // Initialize available weapons based on engagement feasibility
        initializeAvailableWeapons();

        // Initialize target survival probabilities
        for (int j = 0; j < T; j++) {
            targetSurvivalProb[j] = 1.0;
        }

        // Create a list of all possible assignment pairs
        std::vector<AssignmentPair> assignmentPairs;

        for (int t = 0; t < S; t++) {
            for (int j = 0; j < T; j++) {
                for (int i : availableWeapons[j][t]) {
                    // Check if the assignment is feasible based on constraints
                    if (!C2F[i][t] && !C3F[j][t] && !C4F[i]) {
                        double vqpValue = calculateVQP(i, j, t);
                        assignmentPairs.push_back(AssignmentPair(i, j, t, vqpValue));
                    }
                }
            }
        }

        // Sort assignment pairs by VQP value (descending)
        std::sort(assignmentPairs.begin(), assignmentPairs.end());

        // Assign weapons based on sorted pairs
        for (const auto& pair : assignmentPairs) {
            int i = pair.weaponIndex;
            int j = pair.targetIndex;
            int t = pair.stageIndex;

            // Check if the assignment is still feasible
            if (!C2F[i][t] && !C3F[j][t] && !C4F[i]) {
                // Make the assignment
                decision[i][j][t] = true;

                // Update constraint saturation states
                C2F[i][t] = true;  // Weapon i can't be used again at stage t
                C3F[j][t] = true;  // No more weapons can be assigned to target j at stage t

                // Update weapon usage count and check if it's saturated
                int usageCount = 0;
                for (int s = 0; s < S; s++) {
                    for (int k = 0; k < T; k++) {
                        if (decision[i][k][s]) {
                            usageCount++;
                        }
                    }
                }

                if (usageCount >= maxWeaponUses[i]) {
                    C4F[i] = true;  // Weapon i can't be used anymore
                }

                // Update target survival probability
                updateTargetSurvivalProb(j);

                // Recalculate VQP values for remaining pairs with the same target
                for (auto& otherPair : assignmentPairs) {
                    if (otherPair.targetIndex == j && !decision[otherPair.weaponIndex][j][otherPair.stageIndex]) {
                        otherPair.vqpValue = calculateVQP(otherPair.weaponIndex, j, otherPair.stageIndex);
                    }
                }

                // Resort the remaining pairs
                std::sort(assignmentPairs.begin(), assignmentPairs.end());
            }
        }
    }

    // Calculate the objective value (expected total value of surviving assets)
    double getObjectiveValue() {
        double objectiveValue = 0.0;

        // Calculate the expected total value of surviving assets
        for (int k = 0; k < K; k++) {
            double assetSurvivalProb = 1.0;

            for (int j = 0; j < T; j++) {
                // Check if target j threatens asset k
                if (targetLethality[j][k] > 0) {
                    // Calculate probability that target j destroys asset k
                    double targetDestructionProb = targetLethality[j][k] * targetSurvivalProb[j];
                    assetSurvivalProb *= (1.0 - targetDestructionProb);
                }
            }

            objectiveValue += assetValues[k] * assetSurvivalProb;
        }

        return objectiveValue;
    }

    // Get the decision
    const std::vector<std::vector<std::vector<bool>>>& getDecision() const {
        return decision;
    }

    void solveWithCplex() {
        try {
            IloEnv env;
            IloModel model(env);

            // Create decision variables
            IloArray<IloArray<IloNumVarArray>> x(env, W);
            for (int i = 0; i < W; i++) {
                x[i] = IloArray<IloNumVarArray>(env, T);
                for (int j = 0; j < T; j++) {
                    x[i][j] = IloNumVarArray(env, S, 0, 1, ILOBOOL);
                }
            }

            // Create a linearized objective function
            IloExpr objExpr(env);

            // For each asset
            for (int k = 0; k < K; k++) {
                // TO JEST 1 - P
                IloNumVarArray targetSurvival(env, T, 0.0, 1.0);

                // Add constraints to define target survival probabilities
                for (int j = 0; j < T; j++) {
                    if (targetLethality[j][k] > 0) {
                        IloExpr survivalExpr(env);
                        survivalExpr.setConstant(1.0);

                        for (int t = 0; t < S; t++) {
                            for (int i = 0; i < W; i++) {   
                                if (engagementFeasibility[i][j][t]) {
                                    survivalExpr -= weaponLethality[i][j][t] * x[i][j][t];
                                }
                            }
                        }

                        model.add(targetSurvival[j] == survivalExpr);
                        survivalExpr.end();
                    }
                }

                // TO JEST 1 - q !!!!!!
                IloNumVar assetSurvival(env, 0.0, 1.0);

                // Add constraint to define asset survival probability
                IloExpr assetSurvExpr(env);
                assetSurvExpr.setConstant(1.0);

                for (int j = 0; j < T; j++) {
                    if (targetLethality[j][k] > 0) {
                        // Linear approximation of asset survival
                        assetSurvExpr -= targetLethality[j][k] * targetSurvival[j];
                    }
                }

                // Add constraint for asset survival
                model.add(assetSurvival == assetSurvExpr);
                assetSurvExpr.end();

                // Add to objective
                objExpr += assetValues[k] * assetSurvival;
            }

            model.add(IloMaximize(env, objExpr));
            objExpr.end();

            // Rest of the constraints remain the same
            // Constraint set (2): Weapon capability constraints
            for (int i = 0; i < W; i++) {
                for (int t = 0; t < S; t++) {
                    IloExpr expr(env);
                    for (int j = 0; j < T; j++) {
                        expr += x[i][j][t];
                    }
                    model.add(expr <= 1);
                    expr.end();
                }
            }

            // Constraint set (3): Strategy constraints
            for (int j = 0; j < T; j++) {
                for (int t = 0; t < S; t++) {
                    IloExpr expr(env);
                    for (int i = 0; i < W; i++) {
                        expr += x[i][j][t];
                    }
                    model.add(expr <= 1);
                    expr.end();
                }
            }

            // Constraint set (4): Resource constraints
            for (int i = 0; i < W; i++) {
                IloExpr expr(env);
                for (int t = 0; t < S; t++) {
                    for (int j = 0; j < T; j++) {
                        expr += x[i][j][t];
                    }
                }
                model.add(expr <= maxWeaponUses[i]);
                expr.end();
            }

            // Constraint set (5): Engagement feasibility constraints
            for (int i = 0; i < W; i++) {
                for (int j = 0; j < T; j++) {
                    for (int t = 0; t < S; t++) {
                        if (!engagementFeasibility[i][j][t]) {
                            model.add(x[i][j][t] == 0);
                        }
                    }
                }
            }

            // Create solver and solve
            IloCplex cplex(model);

            // Set the starting solution from our heuristic
            IloNumVarArray startVars(env);
            IloNumArray startVals(env);

            for (int i = 0; i < W; i++) {
                for (int j = 0; j < T; j++) {
                    for (int t = 0; t < S; t++) {
                        startVars.add(x[i][j][t]);
                        startVals.add(decision[i][j][t] ? 1.0 : 0.0);
                    }
                }
            }

            cplex.addMIPStart(startVars, startVals);

            // Set CPLEX parameters
            cplex.setParam(IloCplex::TiLim, 60);
            cplex.setParam(IloCplex::Threads, 4);

            // Solve the problem
            if (cplex.solve()) {
                std::cout << "Solution status: " << cplex.getStatus() << std::endl;
                std::cout << "Objective value: " << cplex.getObjValue() << std::endl;

                // Extract the solution
                for (int i = 0; i < W; i++) {
                    for (int j = 0; j < T; j++) {
                        for (int t = 0; t < S; t++) {
                            decision[i][j][t] = (cplex.getValue(x[i][j][t]) > 0.5);
                        }
                    }
                }
            }
            else {
                std::cout << "No solution found." << std::endl;
            }

            env.end();
        }
        catch (IloException& e) {
            std::cerr << "Concert exception caught: " << e << std::endl;
        }
        catch (...) {
            std::cerr << "Unknown exception caught" << std::endl;
        }
    }


    // Fixed engagement feasibility generation
    void generateRandomInstance() {
        std::uniform_real_distribution<double> assetValueDist(10.0, 100.0);
        std::uniform_real_distribution<double> lethalityDist(0.4, 0.9);
        std::uniform_real_distribution<double> targetLethalityDist(0.6, 0.99);
        std::uniform_real_distribution<double> uniformDist(0.0, 1.0);

        // Generate asset values
        for (int k = 0; k < K; k++) {
            assetValues[k] = assetValueDist(rng);
        }

        // Generate target lethality
        for (int j = 0; j < T; j++) {
            int assetIndex = j % K;
            targetLethality[j][assetIndex] = targetLethalityDist(rng);
        }

        // Generate weapon lethality
        for (int i = 0; i < W; i++) {
            for (int j = 0; j < T; j++) {
                for (int t = 0; t < S; t++) {
                    weaponLethality[i][j][t] = lethalityDist(rng);
                }
            }
        }

        // Fixed engagement feasibility generation
        for (int t = 0; t < S; t++) {
            double ratio;
            if (S == 1) {
                ratio = 0.3;
            }
            else {
                ratio = 0.1 + 0.8 * ((double)t / (S - 1));
            }

            for (int i = 0; i < W; i++) {
                for (int j = 0; j < T; j++) {
                    engagementFeasibility[i][j][t] = (uniformDist(rng) > ratio);
                }
            }
        }

        // Generate maximum weapon uses
        for (int i = 0; i < W; i++) {
            maxWeaponUses[i] = 1 + (rand() % S);
        }
    }


    // Print the solution
    void printSolution() {
        std::cout << "Weapon assignments:" << std::endl;
        for (int t = 0; t < S; t++) {
            std::cout << "Stage " << t + 1 << ":" << std::endl;
            bool hasAssignments = false;
            for (int i = 0; i < W; i++) {
                for (int j = 0; j < T; j++) {
                    if (decision[i][j][t]) {
                        std::cout << "  Weapon " << i + 1 << " -> Target " << j + 1
                            << " (VQP: " << calculateVQP(i, j, t) << ")" << std::endl;
                        hasAssignments = true;
                    }
                }
            }
            if (!hasAssignments) {
                std::cout << "  No assignments" << std::endl;
            }
        }

        std::cout << "\nExpected total value of surviving assets: " << getObjectiveValue()
            << " out of " << getTotalAssetValue() << " ("
            << (getObjectiveValue() / getTotalAssetValue() * 100) << "%)" << std::endl;
    }

    // Get total asset value
    double getTotalAssetValue() {
        double total = 0.0;
        for (int k = 0; k < K; k++) {
            total += assetValues[k];
        }
        return total;
    }
};

int main() {
    // Example problem dimensions
    int W = 50;  // Number of weapons
    int T = 50;  // Number of targets
    int K = 10;  // Number of assets
    int S = 4;   // Number of stages

    // Create solver
    DWTASolver solver(W, T, K, S);

    // Generate random problem instance
    solver.generateRandomInstance();

    // Solve using the rule-based heuristic
    auto start = std::chrono::high_resolution_clock::now();
    solver.solve();
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Rule-based heuristic solution time: " << elapsed.count() << " seconds" << std::endl;

    solver.printSolution();

    std::cout << "\nSolving with CPLEX..." << std::endl;
    start = std::chrono::high_resolution_clock::now();
    solver.solveWithCplex();
    end = std::chrono::high_resolution_clock::now();

    elapsed = end - start;
    std::cout << "CPLEX solution time: " << elapsed.count() << " seconds" << std::endl;

    // Print the refined solution
    solver.printSolution();

    return 0;
}
