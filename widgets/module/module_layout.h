#pragma once

#include <list>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include "../../sight_common.h"
#include "../../sight_layout.h"
#include "module_common.h"
#include <gsl/gsl_multifit.h>

namespace sight {
namespace layout {

class moduleLayoutHandlerInstantiator : layoutHandlerInstantiator {
  public:
  moduleLayoutHandlerInstantiator();
};
extern moduleLayoutHandlerInstantiator moduleLayoutHandlerInstance;

class moduleTraceStream;

// Records the information of a given module when the module is entered so that we have it available 
// when the module is exited
class module {
  public:
  std::string moduleName;
  int moduleID;
  int numInputs;
  int numOutputs;
  int count;
  
  module(const std::string& moduleName, int moduleID, int numInputs, int numOutputs, int count) :
    moduleName(moduleName), moduleID(moduleID), numInputs(numInputs), numOutputs(numOutputs), count(count)
  {}
};

class modularApp: public block, public common::module, public traceObserver
{
  friend class moduleTraceStream;
  protected:
 
  // Points to the currently active instance of modularApp. There can be only one.
  static modularApp* activeMA;
    
  // The path the directory where output files of the graph widget are stored
  // Relative to current path
  static std::string outDir;
  // Relative to root of HTML document
  static std::string htmlOutDir;
    
  // Name of this modular app
  std::string appName;
  
  // Unique ID of this modular app
  int appID;
  
  // Stack of the modules that are currently in scope within this modularApp
  static std::list<sight::layout::module> mStack;
  
  // Maps each module group's ID to the trace that holds the observations performed within it
  std::map<int, traceStream*> moduleTraces;
    
  // Maps each traceStream's ID to the ID of its corresponding module graph node
  std::map<int, int> trace2moduleID;
  
  // The dot file that will hold the representation of the module interaction graph
  std::ofstream dotFile;
  
  public:
  
  modularApp(properties::iterator props);
  ~modularApp();
  
  // Returns the unique instance of modularApp currently active
  static modularApp* getInstance() { assert(activeMA); return activeMA; }
  
  // Initialize the environment within which generated graphs will operate, including
  // the JavaScript files that are included as well as the directories that are available.
  static void initEnvironment();
    
  // Registers the ID of the trace that is associated with the current module
  //void registerTraceID(int traceID);
  //static void* registerTraceID(properties::iterator props);
  
  // Do a multi-variate polynomial fit of the data observed for the given nodeID and return for each trace attribute 
  // a string that describes the function that best fits its values
  std::vector<std::string> polyFit(int moduleID);
  
  // Emits to the dot file the buttons used to select the combination of input property and trace attribute
  // that should be shown in the data panel of a given module node.
  // numInputs/numOutputs - the number of inputs/outputs of this module node
  // ID - the unique ID of this module node
  // prefix - We measure both the observations of measurements during the execution of modules and the 
  //    properties of module outputs. Both are included in the trace of the module but the names of measurements
  //    are prefixed with "measure:" and the names of outputs are prefixed with "output:#:", where the # is the
  //    index of the output. The prefix argument identifies the types of attributs we'll be making buttons for and
  //    it should be either "module" or "output".
  // bgColor - The background color of the buttons
  void showButtons(int numInputs, int numOutputs, int ID, std::string prefix, std::string bgColor);
  
  // Enter a new module within the current modularApp
  // numInputs/numOutputs - the number of inputs/outputs of this module node
  // ID - the unique ID of this module node
  void enterModule(std::string node, int moduleID, int numInputs, int numOutputs, int count);
  // Static version of enterModule() that pulls the from/to anchor IDs from the properties iterator and calls 
  // enterModule() in the currently active modularApp
  static void* enterModule(properties::iterator props);
  
  // Exit a module within the current modularApp
  void exitModule();
  // Static version of enterModule() that calls exitModule() in the currently active modularApp
  static void exitModule(void* obj);
  
  // Add a directed edge from the location of the from anchor to the location of the to anchor
  void addEdge(int fromC, common::module::ioT fromT, int fromP, 
               int toC,   common::module::ioT toT,   int toP,
               double prob);
  static void* addEdge(properties::iterator props);
  
  // Called to notify this block that a sub-block was started/completed inside of it. 
  // Returns true of this notification should be propagated to the blocks 
  // that contain this block and false otherwise.
  bool subBlockEnterNotify(block* subBlock) { return false; }
  bool subBlockExitNotify (block* subBlock) { return false; }
  
  protected:
  // Maps each nodeID to the data needed to compute a polynomial approximation of the relationship
  // between its input context and its observations
  
  // Matrix of polynomial terms composed of context values, 1 row per observation, 1 column for each combination of terms
  std::map<int, gsl_matrix*> polyfitCtxt;
  
  // For each value that is observed, a vector of the values actually observed, one entry per observation
  //std::map<int, std::vector<gsl_vector*> >  polyfitObs;
  std::map<int, gsl_matrix*> polyfitObs;
    
  // The number of observations made for each node
  std::map<int, int> numObs;
    
  // The number of observations for which we've allocated space in polyfitCtxt and polyfitObs (the rows)
  std::map<int, int> numAllocObs;
  
  // The number of trace attributes for which we've allocated space in newObs (the columns)
  std::map<int, int> numAllocTraceAttrs;
  
  // Maps the names of trace attributes to their columns in polyfitCtxt
  std::map<int, std::map<std::string, int > > traceAttrName2Col;
  
  // Records the number of observations we've made of each trace attribute (indexed according to the column numbers in traceAttrName2Col)
  std::map<int, std::vector<int> > traceAttrName2Count;
  
  // The number of numeric context attributes of each node. Should be the same for all observations for the node
  std::map<int, int> numNumericCtxt;
  std::map<int, std::list<std::string> > numericCtxtNames;
    
  /* // For each node, for each input, the names of its context attributes
  std::map<int, std::map<int, std::list<std::string> > > ctxtNames;*/
  // For each node, for each grouping of context attributes, the names of all the attributes within the grouping
  std::map<int, std::map<std::string, std::list<std::string> > > ctxtNames;
    
  // The names of the observation trace attributes
  std::map<int, std::set<std::string> > traceAttrNames;
  
  public:
  // Interface implemented by objects that listen for observations a traceStream reads. Such objects
  // call traceStream::registerObserver() to inform a given traceStream that it should observations.
  void observe(int traceID,
               const std::map<std::string, std::string>& ctxt, 
               const std::map<std::string, std::string>& obs,
               const std::map<std::string, anchor>&      obsAnchor,
               const std::set<traceObserver*>&           observers);
}; // class module

// Specialization of traceStreams for the case where they are hosted by a module
class moduleTraceStream: public traceStream
{
  friend class modularApp;
  protected:
  int moduleID;
  std::string name;
  int numInputs;
  int numOutputs;
  
  // The queue that passes all incoming observations through cmFilter and then forwards them to modularApp.
  traceObserverQueue* queue;
  
  public:
  moduleTraceStream(properties::iterator props, traceObserver* observer=NULL);
  ~moduleTraceStream();
  
  // Called when we observe the entry tag of a moduleTraceStream
  static void *enterTraceStream(properties::iterator props);
};

// This class analyzes the observations from a compModuleTraceStream. It relates all observations that share
// the same values for the subset of context attributes that are not in the set options to a single one reference 
// observation (one reference for each value of non-option attributes) and emits these comparisons to the 
// traceObservers that listen to it.
class compModule : public common::module, public traceObserver {
  friend class compModuleTraceStream;
  // Records whether this is the reference configuration of the module
  //bool isReference;
  
  // The context that describes the configuration options of this module
  //context options;
  
  // Maps each configuration of input context values to the mapping of trace attributes to their values 
  // within the reference configuration of the compModule.
  std::map<std::map<std::string, std::string>, std::map<std::string, attrValue> > referenceObs;
  
  // Records for each configuration of input context values all the the mappings of trace and context attributes to
  // their values within non-reference configurations of the compModule. We keep these around until we 
  // find the reference configuration for the given configuration of input context values and once we find 
  // it, we relate these to the reference, emit them to this compModuleTraceStream's observer and empty them out.
  std::map<std::map<std::string, std::string>, std::list<std::map<std::string, attrValue> > > comparisonObs;
  std::map<std::map<std::string, std::string>, std::list<std::map<std::string, std::string> > > comparisonCtxt;
  
  // For each output and name of a context of the output, records a pointer to the comparator to be used
  // for this output context.
  std::vector<std::map<std::string, comparator*> > outComparators;
  
  // Maps the name of each measurement to the pointer to the comparator to be used for this measurement
  std::map<std::string, comparator*> measComparators;

  public:  
  compModule(/*bool isReference, const context& options*/) /*: isReference(isReference), options(options)*/ { }
  
  // Compare the value of each trace attribute value obs1 to the corresponding value in obs2 and return a mapping of 
  // each trace attribute to the serialized representation of their relationship.
  // obs1 and obs2 must have the same trace attributes (map keys).
  std::map<std::string, std::string> compareObservations(
                                           const std::map<std::string, attrValue>& obs1,
                                           const std::map<std::string, attrValue>& obs2);
  
  // Given a mapping of trace attribute names to the serialized representations of their attrValues, returns
  // the same mapping but with the attrValues deserialized as attrValues
  static std::map<std::string, attrValue> deserializeObs(const std::map<std::string, std::string>& obs);
  
  // Interface implemented by objects that listen for observations a traceStream reads. Such objects
  // call traceStream::registerObserver() to inform a given traceStream that it should observations.
  void observe(int traceID,
               const std::map<std::string, std::string>& ctxt, 
               const std::map<std::string, std::string>& obs,
               const std::map<std::string, anchor>&      obsAnchor,
               const std::set<traceObserver*>&           observers);
}; // class compModule

// Specialization of traceStreams for the case where they are hosted by a compModule 
class compModuleTraceStream: public moduleTraceStream
{
  // The object that filters observations to compare non-reference observations to their reference values
  compModule* cmFilter;
  
  public:
  compModuleTraceStream(properties::iterator props, traceObserver* observer=NULL);
  ~compModuleTraceStream();
  
  // Called when we observe the entry tag of a compModuleTraceStream
  static void *enterTraceStream(properties::iterator props);
}; // class compModuleTraceStream


}; // namespace layout
}; // namespace sight