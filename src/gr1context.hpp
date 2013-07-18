//=====================================================================
// General data structures for GR(1) synthesis
//=====================================================================
#ifndef __GR1CONTEXT_HPP__
#define __GR1CONTEXT_HPP__

#include "BF.h"
#include <set>
#include <list>
#include <vector>
#include "bddDump.h"

typedef enum { PreInput, PreOutput, PostInput,PostOutput } VariableType;

/**
 * @brief Container class for all GR(1) synthesis related activities
 *        Modifications of the GR(1) synthesis algorithm
 *        (or strategy extraction) should derive from this class, as it
 *        provides input parsing and BF/BDD book-keeping.
 */
class GR1Context : public VariableInfoContainer {
protected:

    //@{
    /** @name BF-related stuff that is computed in the constructor, i.e., with loading the input specification
     */
    BFManager mgr;
    std::vector<BF> variables;
    std::vector<std::string> variableNames;
    std::vector<VariableType> variableTypes;
    std::vector<BF> livenessAssumptions;
    std::vector<BF> livenessGuarantees;
    BF initEnv;
    BF initSys;
    BF safetyEnv;
    BF safetySys;
    BFVarVector varVectorPre;
    BFVarVector varVectorPost;
    BFVarCube varCubePostInput;
    BFVarCube varCubePostOutput;
    BFVarCube varCubePreInput;
    BFVarCube varCubePreOutput;
    BFVarCube varCubePre;
    std::vector<BF> preVars;
    std::vector<BF> postVars;
    //@}

    //@{
    /** @name Information that is computed during realizability checking
     *  The following variables are set by the realizability checking part. The first one
     *  contains information to be used during strategy extraction - it prodives a sequence
     *  of BFs/BDDs that represent transitions in the game that shall be preferred over those
     *  that come later in the vector. The 'unsigned int' data type in the vector represents the goal
     *  that a BF refers to. The winningPositions BF represents which positions are winning for the system player.
     */
    std::vector<std::pair<unsigned int,BF> > strategyDumpingData;
    bool realizable;
    BF winningPositions;
    //@}

private:
    //! This variable is only used during parsing the input instance.
    //! It allows us to get better error messages for parsing.
    unsigned int lineNumberCurrentlyRead;

    //@{
    /**
     * @name Internal functions - these are used during parsing an input instance
     */
    BF parseBooleanFormulaRecurse(std::istringstream &is,std::set<VariableType> &allowedTypes);
    BF parseBooleanFormula(std::string currentLine,std::set<VariableType> &allowedTypes);
    //@}

public:
    GR1Context(std::list<std::string> &filenames);
    virtual ~GR1Context() {}
    virtual void computeWinningPositions();
    virtual void checkRealizability();
    virtual void execute();
    static BF determinize(BF in, std::vector<BF> vars);

    //@{
    /**
     * @name The following functions are inherited from the VariableInfo container.
     * They allow us to the the BF_dumpDot function for debugging new variants of the synthesis algorithm
     */
    void getVariableTypes(std::vector<std::string> &types) const {
        types.push_back("PreInput");
        types.push_back("PreOutput");
        types.push_back("PostInput");
        types.push_back("PostOutput");
    }

    void getVariableNumbersOfType(std::string typeString, std::vector<unsigned int> &nums) const {
        VariableType type;
        if (typeString=="PreInput") type = PreInput;
        else if (typeString=="PreOutput") type = PreOutput;
        else if (typeString=="PostInput") type = PostInput;
        else if (typeString=="PostOutput") type = PostOutput;
        else throw "Cannot detect variable type for BDD dumping";
        for (unsigned int i=0;i<variables.size();i++) {
            if (variableTypes[i] == type) nums.push_back(i);
        }
    }

    virtual BF getVariableBF(unsigned int number) const {
        return variables[number];
    }

    virtual std::string getVariableName(unsigned int number) const {
        return variableNames[number];
    }
    //@}

    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new GR1Context(filenames);
    }

};


/**
 * @brief Helper class for easier BF-based fixed point computation
 */
class BFFixedPoint {
private:
    BF currentValue;
    bool reachedFixedPoint;
public:
    BFFixedPoint(BF init) : currentValue(init), reachedFixedPoint(false) {}
    void update(BF newOne) {
        if (currentValue == newOne) {
            reachedFixedPoint = true;
        } else {
            currentValue = newOne;
        }
    }
    bool isFixedPointReached() const { return reachedFixedPoint; }
    BF getValue() { return currentValue; }
};

/**
 * @brief A customized class for Exceptions in Slugs - Can trigger printing the comman line parameters of Slugs
 */
class SlugsException {
private:
    std::ostringstream message;
    bool shouldPrintUsage;
public:
    SlugsException(bool _shouldPrintUsage) : shouldPrintUsage(_shouldPrintUsage) {}
    SlugsException(bool _shouldPrintUsage, std::string msg) : shouldPrintUsage(_shouldPrintUsage) { message << msg; }
    SlugsException(SlugsException const &other) : shouldPrintUsage(other.shouldPrintUsage) { message << other.message.str();}
    bool getShouldPrintUsage() const { return shouldPrintUsage; }
    SlugsException& operator<<(const std::string str) { message << str; return *this; }
    SlugsException& operator<<(const double str) { message << str; return *this; }
    SlugsException& operator<<(const int str) { message << str; return *this; }
};


#endif
