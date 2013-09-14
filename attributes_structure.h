#pragma once

#include <list>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>

/*
This file implements support for attributes. An attribute is a key->value mapping that is set by the application. Different mappings may 
exist during different regions of debug output. They are used to 1. control which text is emitted by dbglog (less emitted text means 
better performance) and 2. to tag the emitted blocks so that users can select the ones they wish to view in the browser. Attributes 
provide a flexible mechanism to control which blocks they wish to view based on constraints on the values of keys that they declare.
A simple use-case would be an application that has multiple debug levels. It sets the debug level at the start of the execution, recording
it as an attribute, and then uses attribute operations to indicate the blocks that have debug levels higher than the currently selected 
one and should thus not be emitted into the output. Alternately, all blocks may be emitted and the filtering can be done manually by the 
user from inside the browser.

Attributes are created by declaring objects of type attr:
  attr keyvalAttribute("key", (long)value);
The user provides the attribute's name and value. Attribute values may be one of the following types: string, void*, long or double.
Where there is ambiguity users are expected to cast the value to the type they desire. The key->value mapping is active for as long
as the variable is in scope. It is possible to associate multiple values with the same key.

Whethe debug blocks are emitted is controlled by declaring query objects:
  attrIf aif(attrEQ("key", (long)testValue, attrOp::any));
This indicates that while this object is in scope debug output is only emitted if Any value mapped to the key is Equal to testValue.
The above declaration contains several components:
  - query type (attrIf, attrAnd, attrOr)
  - operation (attrEQ, attrNEQ, attrLT, attrLE, attrGT, attrGE, attrRange)
  - key name
  - test value
  - collection type (attrOp::any or attrOp::all)

Queries are constructed as a sequence that is defined by the nesting structure of query objects. They denote
formulas of form ((key0 RelOp val0) LogOp ((key1 RelOp val1) LogOp ((key2 RelOp val2) LogOp ... True)))
Note that the deepest term in every formula is True, meaning that if no queries are specified debug output is emitted.
  example:
  {attrAnd i(attrEq("key0", (long)testValue0));
    // debug output is emitted here only if(key0 = testValue0 AND True)
    {attrIf i(attrEq("key1", (long)testValue1));
      // debug output is emitted here only if(key1 = testValue1)
      {attrAnd a(attrEq("key2", (long)testValue2));
        // debug output is emitted here only if(key2 = testValue2 AND key1 = testValue1)
        {attrOr o(attrEq("key3", (long)testValue3));
           // debug output is emitted here only if(key3=testValue3 OR (key2 = testValue2 AND key1 = testValue1))
        }
      }
    }
  }
  // debug output here is always emitted
Query Types:
  attrAnd/attrOr: Ands/Ors a new operation to the existing query
  attrIf: Ignores the existing query and begins a new query with the given operation as its deepest term
Operations: 
  attrEQ, attrNEQ, attrLT, attrLE, attrGT, attrGE: equalities and inequalities that work on all value types
  attrRange: takes as input two numbers lb, ub of type long or double and returns true if a given key's value is in range [lb, ub) (lb<=val<ub)
Collection types: each key may be mapped to more than one value.
  attrOp::any: operation returns true if it evaluates to true for ANY value associated with the key
  attrOp::all: operation returns true if it evaluates to true for ALL values associated with the key
*/

namespace dbglog {
namespace structure{

class attrOp;

// ****************************
// ***** Attribute Values *****
// ****************************

// Wrapper class for strings, integers and floating point numbers that keeps track of the 
// type of its contents and allows functors that work on only one of these types to be applied.
class attrValue {
  public:
  typedef enum {strT, ptrT, intT, floatT, unknownT} valueType;
  
  friend class attrOp;
  
  // Storage for any one of the different types of values that attrValue may wrap
  /*typedef union {
    std::string strV;
    void* ptrV;
    long intV;
    double floatV;
  } valueStore;*/
  
  // The type of the value's contents
  valueType type;
  
  // The storage for the four possible value types
  void* store;
  
  public:
  attrValue();
  attrValue(const std::string& strV);
  attrValue(char* strV);
  attrValue(void* ptrV);
  attrValue(long intV);
  attrValue(double floatV);
  attrValue(const attrValue& that);
  ~attrValue();
  
  attrValue& operator=(const attrValue& that);
  
  // Returns the type of this attrValue's contents
  valueType getType() const;
  
  // Return the contents of this attrValue, aborting if there is a type incompatibility.
  std::string getStr() const;
  void*       getPtr() const;
  long        getInt() const;
  double      getFloat() const;
  
  // Encodes the contents of this attrValue into a string and returns the result.
  std::string getAsStr() const;
  
  // Implementations of the relational operators
  bool operator==(const attrValue& that) const;
  bool operator<(const attrValue& that) const;
  bool operator!=(const attrValue& that) const
  { return !(*this == that); }
  bool operator<=(const attrValue& that) const
  { return *this < that || *this == that; }
  bool operator>(const attrValue& that) const
  { return !(*this == that) && !(*this < that); }
  bool operator>=(const attrValue& that) const
  { return (*this == that) || !(*this < that); }
  
  // Returns a human-readable representation of this object
  std::string str() const;
};

// ********************************
// ***** Attribute Operations *****
// ********************************

// An operation that may be applied to an attrValue to probe its contents
class attrOp
{
  // The key that is being evaluated
  std::string key;
  
  // All/Any mode: the result of applying the operation is true only if it is true for All/Any the values associated with some key
  // Any mode: 
  public:
  typedef enum {any, all} applyType;
  private:
  applyType type;
  
  public:
  attrOp(std::string key, applyType type) : key(key), type(type) {}
  
  // For each type of value the functor must provide an implements*() method that 
  // returns whether the functor is applicable to this value type and an apply*()
  // method that can actually be applied to values of this type.
  
  virtual bool implementsString() const { return false; }
  virtual bool applyString(std::string& val) const { throw "Cannot call attrOp::applyString()!"; }
  
  virtual bool implementsPtr() const { return false; }
  virtual bool applyPtr(void*& val) const { throw "Cannot call attrOp::applyPtr()!";  }
  
  virtual bool implementsInt() const { return false; }
  virtual bool applyInt(long& val) const { throw "Cannot call attrOp::applyInt()!";  }
  
  virtual bool implementsFloat() const { return false; }
  virtual bool applyFloat(double& val) const { throw "Cannot call attrOp::applyFloat()!";  }
  
  // Applies the given functor to this given value. Throws an exception if the functor
  // is not applicable to this value type.
  bool apply() const;
  
  // Returns a human-readable representation of this object
  virtual std::string str() const=0;
};

// Base class for attrOps that support all value types
class universalAttrOp : public attrOp
{
  public:
  universalAttrOp(std::string key, applyType type) : attrOp(key, type) {}
  
  bool implementsString() const { return true; }
  bool implementsPtr()    const { return true; }
  bool implementsInt()    const { return true; }
  bool implementsFloat()  const { return true; } 
};

// Special operation that always returns true and is used in cases where we need a no-op
class attrNullOp : public universalAttrOp
{
  public:
  attrNullOp() : universalAttrOp("", any) {}
  bool applyString(std::string& that) const { return true; }
  bool applyPtr(void*& that)          const { return true; }
  bool applyInt(long& that)           const { return true; }
  bool applyFloat(double& that)       const { return true; }
  
  // Returns a human-readable representation of this object
  std::string str() const { return "attrNullOp"; }
};
extern attrNullOp NullOp;

// = Equality test
class attrEQ : public universalAttrOp
{
  attrValue val;
  public:
  attrEQ(std::string key, const attrValue&   val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrEQ(std::string key, const std::string& val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrEQ(std::string key, char*              val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrEQ(std::string key, void*              val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrEQ(std::string key, long               val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrEQ(std::string key, double             val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  
  bool applyString(std::string& that) const { return attrValue(that) == val; }
  bool applyPtr(void*& that)          const { return attrValue(that) == val; }
  bool applyInt(long& that)           const { return attrValue(that) == val; }
  bool applyFloat(double& that)       const { return attrValue(that) == val; }
  
  // Returns a human-readable representation of this object
  std::string str() const { return "attrEQ"; }
};

// != Equality test
class attrNEQ : public universalAttrOp
{
  attrValue val;
  public:
  attrNEQ(std::string key, const attrValue&   val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrNEQ(std::string key, const std::string& val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrNEQ(std::string key, char*              val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrNEQ(std::string key, void*              val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrNEQ(std::string key, long               val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrNEQ(std::string key, double             val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  
  bool applyString(std::string& that) const { return attrValue(that) != val; }
  bool applyPtr(void*& that)          const { return attrValue(that) != val; }
  bool applyInt(long& that)           const { return attrValue(that) != val; }
  bool applyFloat(double& that)       const { return attrValue(that) != val; }
  
  // Returns a human-readable representation of this object
  std::string str() const { return "attrNEQ"; }
};

// < Inequality test
class attrLT : public universalAttrOp
{
  attrValue val;
  public:
  attrLT(std::string key, const attrValue&   val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrLT(std::string key, const std::string& val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrLT(std::string key, char*              val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrLT(std::string key, void*              val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrLT(std::string key, long               val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrLT(std::string key, double             val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  
  bool applyString(std::string& that) const { return attrValue(that) < val; }
  bool applyPtr(void*& that)          const { return attrValue(that) < val; }
  bool applyInt(long& that)           const { return attrValue(that) < val; }
  bool applyFloat(double& that)       const { return attrValue(that) < val; }
  
  // Returns a human-readable representation of this object
  std::string str() const { return "attrLT"; }
};

// <= Inequality test
class attrLE : public universalAttrOp
{
  attrValue val;
  public:
  attrLE(std::string key, const attrValue&   val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrLE(std::string key, const std::string& val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrLE(std::string key, char*              val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrLE(std::string key, void*              val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrLE(std::string key, long               val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrLE(std::string key, double             val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  
  bool applyString(std::string& that) const { return attrValue(that) <= val; }
  bool applyPtr(void*& that)          const { return attrValue(that) <= val; }
  bool applyInt(long& that)           const { return attrValue(that) <= val; }
  bool applyFloat(double& that)       const { return attrValue(that) <= val; }
  
  // Returns a human-readable representation of this object
  std::string str() const { return "attrLE"; }
};

// > Inequality test
class attrGT : public universalAttrOp
{
  attrValue val;
  public:
  attrGT(std::string key, const attrValue&   val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrGT(std::string key, const std::string& val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrGT(std::string key, char*              val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrGT(std::string key, void*              val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrGT(std::string key, long               val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrGT(std::string key, double             val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  
  bool applyString(std::string& that) const { return attrValue(that) > val; }
  bool applyPtr(void*& that)          const { return attrValue(that) > val; }
  bool applyInt(long& that)           const { return attrValue(that) > val; }
  bool applyFloat(double& that)       const { return attrValue(that) > val; }
  
  // Returns a human-readable representation of this object
  std::string str() const { return "attrGT"; }
};

// >= Inequality test
class attrGE : public universalAttrOp
{
  attrValue val;
  public:
  attrGE(std::string key, const attrValue&   val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrGE(std::string key, const std::string& val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrGE(std::string key, char*              val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrGE(std::string key, void*              val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrGE(std::string key, long               val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  attrGE(std::string key, double             val, applyType type=attrOp::any) : universalAttrOp(key, type), val(val) {}
  
  bool applyString(std::string& that) const { /*std::cout << "GE: val="<<val.str()<<" >= that="<<that<<std::endl;*/ return attrValue(that) >= val; }
  bool applyPtr(void*& that)          const { /*std::cout << "GE: val="<<val.str()<<" >= that="<<that<<std::endl;*/ return attrValue(that) >= val; }
  bool applyInt(long& that)           const { /*std::cout << "GE: val="<<val.str()<<" >= that="<<that<<std::endl;*/ return attrValue(that) >= val; }
  bool applyFloat(double& that)       const { /*std::cout << "GE: val="<<val.str()<<" >= that="<<that<<std::endl;*/ return attrValue(that) >= val; }
  
  // Returns a human-readable representation of this object
  std::string str() const { return "attrGE"; }
};

// Range test. Checks that val is in range [lb, ub)
class attrRange : public attrOp
{
  attrValue lb;
  attrValue ub;
  
  public:
  attrRange(std::string key, long   lb, long   ub, applyType type) : attrOp(key, type), lb(lb), ub(ub) {}
  attrRange(std::string key, double lb, double ub, applyType type) : attrOp(key, type), lb(lb), ub(ub) {}
  
  bool implementsInt()    const { return true; }
  bool implementsFloat()  const { return true; }
  
  bool applyInt(long& that)     const;
  bool applyFloat(double& that) const;
  
  // Returns a human-readable representation of this object
  std::string str() const { return "attrRange"; }
};

class attributesC;

// *****************************
// ***** Attribute Queries *****
// *****************************

// An attrQuery implements queries that may be performed on attributes. A query is a list of subQuery objects,
// each of which can evaluate to true or false and optionally propagate the query to its 
// predecessor in the list.
class attrQuery;

class attrSubQuery
{
  protected:
  // The operation that will be performed on the value associated with the key
  attrOp* op;
  
  // The predecessor of this query in the query chain
  attrSubQuery* pred;
  
  friend class attrQuery;
  
  public:
  attrSubQuery(attrOp* op) : op(op) {}
  ~attrSubQuery() { delete op; }
  
  // Performs the query on either the given attributes object or the one defined globally
  virtual bool query(const attributesC& attr)=0;
  bool query();
};  // class attrSubQuery

class attrQuery
{
  // Points to the last query in the linked list of queries
  attrSubQuery* lastQ;
  
  public:
  attrQuery();
  
  // Adds the given sub-query to the list of queries
  void push(attrSubQuery* subQ);
  
  // Removes the last sub-query from the list of queries
  void pop();
  
  // Returns the result of this query on the current state of the given attributes object
  bool query(const attributesC& attr);
}; // class attrQuery

class attrSubQueryAnd : public attrSubQuery
{
  public:
  attrSubQueryAnd(attrOp* op) : attrSubQuery(op) {}
    
  // Applies the operator to the values at the given key. The && ensures that if the operator returns true,
  // the query is propagated to the previous attrSubQuery object. If the previous object is NULL, returns true.
  bool query(const attributesC& attr);
};

class attrSubQueryOr : public attrSubQuery
{
  public:
  attrSubQueryOr(attrOp* op) : attrSubQuery(op) {}
    
  // Applies the operator to the values at the given key. The || ensures that if the operator returns false,
  // the query is propagated to the previous attrSubQuery object. If the previous object is NULL, returns false.
  bool query(const attributesC& attr);
};

class attrSubQueryIf : public attrSubQuery
{
  public:
  attrSubQueryIf(attrOp* op) : attrSubQuery(op) {}
  
  // Applies the operator to the values at the given key, returning its result. This object never propagates
  // queries to its predecessors.
  bool query(const attributesC& attr);
};

class attrSubQueryTrue : public attrSubQuery
{
  public:
  attrSubQueryTrue() : attrSubQuery(&NullOp) {}
  
  // Always returns true
  bool query(const attributesC& attr);
};

class attrSubQueryFalse : public attrSubQuery
{
  public:
  attrSubQueryFalse() : attrSubQuery(&NullOp) {}
  
  // Always returns false
  bool query(const attributesC& attr);
};

// ******************************
// ***** Attribute Database *****
// ******************************

// Interface implemented by objects that wish to listen for changes to mappings of a given key
class attrObserver {
  public:
  // Called before key's mapping is changed
  virtual void observePre(std::string key) { }
    
  // Called after key's mapping is changed
  virtual void observePost(std::string key) { }
};

// Maintains the mapping from atribute keys to values
class attributesC
{
  // --- STORAGE ---
  private:
  std::map<std::string, std::set<attrValue> > m;
  
  // Maps each key to a all the attrObserver objects that observe changes in its mappings.
  // We map each observer to the number of times it has been added to make it possible to 
  // add an observer multiple times as long as it is removed the same number of times.
  std::map<std::string, std::map<attrObserver*, int> > o;
   
  // Adds the given value to the mapping of the given key without removing the key's prior mapping.
  // Returns true if the attributes map changes as a result and false otherwise.
  public:
  bool add(std::string key, std::string val);
  bool add(std::string key, char* val);
  bool add(std::string key, void* val);
  bool add(std::string key, long val);
  bool add(std::string key, double val);
  bool add(std::string key, const attrValue& val);
  
  // Adds the given value to the mapping of the given key, while removing the key's prior mapping, if any.
  // Returns true if the attributes map changes as a result and false otherwise.
  public:
  bool replace(std::string key, std::string val);
  bool replace(std::string key, char* val);
  bool replace(std::string key, void* val);
  bool replace(std::string key, long val);
  bool replace(std::string key, double val);
  bool replace(std::string key, const attrValue& val);
  
  // Returns whether this key is mapped to a value
  bool exists(std::string key) const;
    
  // Returns the value mapped to the given key
  const std::set<attrValue>& get(std::string key) const;
  
  // Removes the mapping from the given key to the given value.
  // Returns true if the attributes map changes as a result and false otherwise.
  public:
  bool remove(std::string key, std::string val);
  bool remove(std::string key, char* val);
  bool remove(std::string key, void* val);
  bool remove(std::string key, long val);
  bool remove(std::string key, double val);
  bool remove(std::string key, const attrValue& val);
  
  // Removes the mapping of this key to any value.
  // Returns true if the attributes map changes as a result and false otherwise.
  public:
  bool remove(std::string key);
  
  // These routines manage the mapping from keys to the objects that observe changes in them
  
  // Add a given observer for the given key
  void addObs(std::string key, attrObserver* obs);
  
  // Remove a given observer from the given key
  void remObs(std::string key, attrObserver* obs);
    
  // Remove all observers from a given key
  void remObs(std::string key);
  
  private:
  // Notify all the observers of the given key before its mapping is changed (call attrObserver::observePre())
  void notifyObsPre(std::string key);
  // Notify all the observers of the given key after its mapping is changed (call attrObserver::observePost())
  void notifyObsPost(std::string key);
  
  // --- QUERYING ---
  private:
  // The current query that is being evaluates against this attributes map
  attrQuery q;
  
  // The most recent return value of the query object q
  bool lastQRet;
  
  // Records whether 
  // - this query object's most recent return value is current (neither q nor m have changed), in which case we 
  //   can respond to the next query with lastQRet, 
  // - or not, in which case we have to fully execute the next query
  bool qCurrent;
  
  public:
  // Adds the given sub-query to the list of queries
  void push(attrSubQuery* subQ);
  
  // Removes the last sub-query from the list of queries
  void pop();
  
  // Returns the result of the current query q on the current state of this attributes object
  bool query();
  
  public:
  attributesC() {
    // Queries on an empty attributes object evaluate to true by default (by default we emit debug output)
    lastQRet = true;
    qCurrent = true;
  }
  
  // Returns a representation of the attributes database as a JavaScript map
  std::string strJS() const;
  
}; // class attributes

extern attributesC attributes;

// *******************************
// ***** Attribute Interface *****
// *******************************

// C++ interface for attribute creation, destruction
class attr
{
  // The key/value of this attribute
  std::string key;
  attrValue val;
    
  // Records whether the value that this attribute's key was assigned to before the attribute was set
  bool keyPreviouslySet;
  
  // The value that this attribute's key was assigned to before the attribute was set, if any
  attrValue oldVal;
  
  public:
  attr(std::string key, std::string val);
  attr(std::string key, char*       val);
  attr(std::string key, void*       val);
  attr(std::string key, long        val);
  attr(std::string key, double      val);
  
  template<typename T>
  void init(std::string key, T val);
  
  ~attr();
  
  // Returns the key of this attribute
  std::string getKey() const;
  
  // Returns the value of this attribute
  const attrValue& getVal() const;
  
  // Returns the type of this attribute values's contents
  attrValue::valueType getVType() const;
  
  // Return the contents of this attribute values, aborting if there is a type incompatibility
  std::string getVStr() const;
  void*       getVPtr() const;
  long        getVInt() const;
  double      getVFloat() const;
  
  // Implementations of the relational operators
  bool operator==(const attr& that) const;
  bool operator<(const attr& that) const;
  bool operator!=(const attr& that) const
  { return !(*this == that); }
  bool operator<=(const attr& that) const
  { return *this < that || *this == that; }
  bool operator>(const attr& that) const
  { return !(*this == that) && !(*this < that); }
  bool operator>=(const attr& that) const
  { return (*this == that) || !(*this < that); }
};

// C++ interface for query creation, destruction

class attrAnd: public attrSubQueryAnd { 
  public:
  attrAnd(attrOp* op) : attrSubQueryAnd(op) 
  { attributes.push(this); }
  ~attrAnd() { attributes.pop(); }
};

class attrOr: public attrSubQueryOr { 
  public:
  attrOr(attrOp* op) : attrSubQueryOr(op) 
  { attributes.push(this); }
  ~attrOr() { attributes.pop(); }
};

class attrIf: public attrSubQueryIf { 
  public:
  attrIf(attrOp* op) : attrSubQueryIf(op) 
  { attributes.push(this); }
  ~attrIf() { attributes.pop(); }
};

class attrTrue: public attrSubQueryTrue { 
  public:
  attrTrue(attrOp* op) : attrSubQueryTrue() 
  { attributes.push(this); }
  ~attrTrue() { attributes.pop(); }
};
  
class attrFalse: public attrSubQueryFalse { 
  public:
  attrFalse(attrOp* op) : attrSubQueryFalse() 
  { attributes.push(this); }
  ~attrFalse() { attributes.pop(); }
};
  
}; // namespace structure
}; // namespace dbglog
