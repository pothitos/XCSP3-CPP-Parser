/*=============================================================================
 * parser for CSP instances represented in XCSP3 Format
 *
 * Copyright (c) 2015 xcsp.org (contact <at> xcsp.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *=============================================================================
 */

#include <map>

#include "XCSP3Tree.h"
#include "XCSP3TreeNode.h"
#include <sstream>
#include <vector>
#include <limits>
#include <algorithm>

using namespace XCSP3Core;


static bool isSymmetricOperator(ExpressionType type) {
    return type == OADD || type == OMUL || type == OMIN || type == OMAX || type == ODIST || type == ONE
           || type == OEQ || type == OSET || type == OAND || type == OOR || type == OXOR || type == OIFF ||
           type == OUNION || type == OINTER || type == ODJOINT;
}


static bool isNonSymmetricRelationalOperator(ExpressionType type) {
    return type == OLT || type == OLE || type == OGE || type == OGT;
}


static ExpressionType arithmeticInversion(ExpressionType type) {
    return type == OLT ? OGT : type == OLE ? OGE : type == OGE ? OLE : type == OGT ? OLT : type; // no change for NE and EQ
}


static std::string operatorToString(ExpressionType op) {
    if(op == ONEG) return "neg";
    if(op == OABS) return "abs";

    if(op == OADD) return "add";
    if(op == OSUB) return "sub";
    if(op == OMUL) return "mul";
    if(op == ODIV) return "div";
    if(op == OMOD) return "mod";

    if(op == OSQR) return "sqr";
    if(op == OPOW) return "pow";

    if(op == OMIN) return "min";
    if(op == OMAX) return "max";

    if(op == ODIST) return "dist";

    if(op == OLE) return "le";
    if(op == OLT) return "lt";
    if(op == OGE) return "ge";
    if(op == OGT) return "gt";

    if(op == ONE) return "ne";
    if(op == OEQ) return "eq";

    if(op == ONOT) return "not";
    if(op == OAND) return "and";
    if(op == OOR) return "or";
    if(op == OXOR) return "xor";
    if(op == OIMP) return "imp";
    if(op == OIF) return "if";
    if(op == OIFF) return "iff";

    if(op == OIN) return "in";
    if(op == ONOTIN) return "notin";
    if(op == OSET) return "set";
    //assert(false);
    return "oundef";
}


NodeOperator *createNodeOperator(std::string op) {
    NodeOperator *tmp = nullptr;
    if(op == "neg") tmp = new NodeNeg();
    if(op == "abs") tmp = new NodeAbs();

    if(op == "add") tmp = new NodeAdd();
    if(op == "sub") tmp = new NodeSub();
    if(op == "mul") tmp = new NodeMult();
    if(op == "div") tmp = new NodeDiv();
    if(op == "mod") tmp = new NodeMod();

    if(op == "sqr") tmp = new NodeSquare();
    if(op == "pow") tmp = new NodePow();

    if(op == "min") tmp = new NodeMin();
    if(op == "max") tmp = new NodeMax();
    if(op == "dist") tmp = new NodeDist();

    if(op == "le") tmp = new NodeLE();
    if(op == "lt") tmp = new NodeLT();
    if(op == "ge") tmp = new NodeGE();
    if(op == "gt") tmp = new NodeGT();

    if(op == "ne") tmp = new NodeNE();
    if(op == "eq") tmp = new NodeEQ();

    if(op == "not") tmp = new NodeNot();
    if(op == "and") tmp = new NodeAnd();
    if(op == "or") tmp = new NodeOr();
    if(op == "xor") tmp = new NodeXor();
    if(op == "imp") tmp = new NodeImp();
    if(op == "if") tmp = new NodeIf();
    if(op == "iff") tmp = new NodeIff();

    if(op == "in") tmp = new NodeIn();
    if(op == "notin") tmp = new NodeNotIn();
    if(op == "set") tmp = new NodeSet();

    assert(tmp != nullptr);
    return tmp;
}


static ExpressionType logicalInversion(ExpressionType type) {
    return type == OLT ? OGE
                            : type == OLE ? OGT
                                               : type == OGE ? OLT
                                                                  : type == OGT ? OLE
                                                                                     : type == ONE ? OEQ
                                                                                                        : type == OEQ ? ONE
                                                                                                                           : type == OIN ? ONOTIN
                                                                                                                                              : type ==
                                                                                                                                                ONOTIN ? OIN
                                                                                                                                                       :
                                                                                                                                                type ==
                                                                                                                                                OSUBSET
                                                                                                                                                ? OSUPSEQ
                                                                                                                                                :
                                                                                                                                                type ==
                                                                                                                                                OSUBSEQ
                                                                                                                                                ? OSUPSET
                                                                                                                                                :
                                                                                                                                                type ==
                                                                                                                                                OSUPSEQ
                                                                                                                                                ? OSUBSET :
                                                                                                                                                type ==
                                                                                                                                                OSUPSET
                                                                                                                                                ? OSUBSEQ
                                                                                                                                                : OUNDEF;
}


static bool isRelationalOperator(ExpressionType type) {
    return isNonSymmetricRelationalOperator(type) || type == ONE || type == OEQ;
}


bool compareNodes(Node *a, Node *b) {
    if(a->type != b->type)
        return static_cast<int>(a->type)< static_cast<int>(b->type);

    NodeConstant *c1 = dynamic_cast<NodeConstant *>(a), *c2 = dynamic_cast<NodeConstant *>(b);
    if(c1 != nullptr)
        return c1->val < c2->val;

    NodeVariable *v1 = dynamic_cast<NodeVariable *>(a), *v2 = dynamic_cast<NodeVariable *>(b);
    if(v1 != nullptr)
        return v1->var.compare(v2->var) < 0;

    NodeOperator *o1 = dynamic_cast<NodeOperator *>(a), *o2 = dynamic_cast<NodeOperator *>(b);
    if(o1->parameters.size() < o2->parameters.size())
        return 1;
    if(o1->parameters.size() > o2->parameters.size())
        return 0;

    for(int i = 0 ; i < o1->parameters.size() ; i++)
        if((compareNodes(o1->parameters[i], o2->parameters[i])) == 1)
            return 1;
    return 0;
}


Node *NodeOperator::canonize() {
    std::vector<Node *> newParams;
    for(Node *n : parameters)
        newParams.push_back(n->canonize());

    if(isSymmetricOperator(type))
        std::sort(newParams.begin(), newParams.end(), compareNodes);


    ExpressionType newType = type;

    // sons are potentially sorted if the type corresponds to a non-symmetric binary relational operator (in that case, we swap sons and
    // arithmetically
    // inverse the operator)
    if(newParams.size() == 2 && isNonSymmetricRelationalOperator(type) &&
       (static_cast<int>(arithmeticInversion(type)) < static_cast<int>(type)
        || (arithmeticInversion(type) == type && compareNodes(newParams[0], newParams[1]) > 0))) {
        newType = arithmeticInversion(type);
        Node *tmp = newParams[0];
        newParams[0] = newParams[1];
        newParams[1] = tmp;
    }
    // Now, some specific reformulation rules are applied
    NodeOperator *tmp = dynamic_cast<NodeOperator *>(newParams[0]);
    if(newType == OABS && newParams[0]->type == OSUB)
        return (new NodeDist())->addParameters(tmp->parameters);

    if(newType == ONOT && newParams[0]->type == ONOT)
        return tmp->parameters[0];

    if(newType == ONEG && newParams[0]->type == ONEG) // neg(neg(...)) becomes ...
        return tmp->parameters[0];

    if(newType == ONOT && logicalInversion(newParams[0]->type) != OUNDEF) // not(lt(...)) becomes ge(...), not(eq(...)) becomes ne(...), and
        return createNodeOperator(operatorToString(logicalInversion(newParams[0]->type)))->addParameters(tmp->parameters);


    if(newParams.size() == 1 && (newType == OADD || newType == OMUL || newType == OMIN || newType == OMAX || newType == OEQ || newType == OAND
                                 || newType == OOR || newType == OXOR || newType == OIFF)) // certainly can happen during the canonization process
        return newParams[0];

    if(newType == OADD) {// we merge constant (similar operations possible for MUL, MIN, ...)
        // They are at the end of the add
        NodeConstant *c1, *c2;
        if(newParams.size() >= 2 && (c1 = dynamic_cast<NodeConstant *>(newParams[newParams.size() - 1])) != nullptr &&
           (c2 = dynamic_cast<NodeConstant *>(newParams[newParams.size() - 2])) != nullptr) {
            std::vector<Node *> l;
            l.insert(l.end(), newParams.begin(), newParams.end() - 2);
            l.push_back(new NodeConstant(c1->val + c2->val));
            return ((new NodeAdd())->addParameters(l))->canonize();
        }
    }
    // Then, we merge operators when possible; for example add(add(x,y),z) becomes add(x,y,z)
    if(isSymmetricOperator(type) && newType != OEQ && newType != ODIST && newType != ODJOINT) {
        for(int i = 0 ; i < newParams.size() ; i++) {
            NodeOperator *n;
            if((n = dynamic_cast<NodeOperator *>(newParams[i])) != nullptr && n->type == newType) {
                std::vector<Node *> list;
                list.insert(list.end(), newParams.begin(), newParams.begin() + i - 1);
                list.insert(list.end(), n->parameters.begin(), n->parameters.end());
                list.insert(list.end(), newParams.begin() + i + 1, newParams.end());
                return ((createNodeOperator(operatorToString(newType)))->addParameters(list))->canonize();

                /*List<XNode<V>> list = IntStream.rangeClosed(0, i - 1).mapToObj(j -> newParams[j]).collect(Collectors.toList());
                Stream.of(((XNodeParent<V>) newParams[i]).sons).forEach(s -> list.add(s));
                IntStream.range(i + 1, newParams.length).mapToObj(j -> newParams[j]).forEach(s -> list.add(s));
                return new XNodeParent<V>(newType, list).canonization();*/
            }
        }
    }
    if(newParams.size() == 2 && isRelationalOperator(type)) {
        NodeOperator *n0 = dynamic_cast<NodeOperator *>(newParams[0]);
        NodeOperator *n1 = dynamic_cast<NodeOperator *>(newParams[1]);
        // First, we replace sub by add when possible
        if(newParams[0]->type == OSUB && newParams[1]->type == OSUB) {
            Node *a = (new NodeAdd())->addParameter(n0->parameters[0])->addParameter(n1->parameters[1]);
            Node *b = (new NodeAdd())->addParameter(n1->parameters[0])->addParameter(n0->parameters[1]);
            return (createNodeOperator(operatorToString(newType)))->addParameter(a)->addParameter(b)->canonize();
        } else if(newParams[1]->type == OSUB) {
            Node *a = (new NodeAdd())->addParameter(newParams[0])->addParameter(n1->parameters[1]);
            Node *b = n1->parameters[0];
            return (createNodeOperator(operatorToString(newType)))->addParameter(a)->addParameter(b)->canonize();
        } else if(n0 != nullptr && n0->op == "sub") {
            Node *a = n0->parameters[0];
            Node *b = (new NodeAdd())->addParameter(newParams[1])->addParameter(n0->parameters[1]);
            return (createNodeOperator(operatorToString(newType)))->addParameter(a)->addParameter(b)->canonize();
        }

        // next, we remove some add when possible
        if (newParams[0]->type == OADD && newParams[1]->type == ODECIMAL) {
            if (n0->parameters.size() == 2 && n0->parameters[0]->type == OVAR && n0->parameters[1]->type == ODECIMAL) {
                NodeConstant *c1 = dynamic_cast<NodeConstant*>(newParams[1]);
                NodeConstant *c2 = dynamic_cast<NodeConstant*>(n0->parameters[1]);
                return (createNodeOperator(operatorToString(newType)))->addParameter(n0->parameters[0])->addParameter(new NodeConstant(c1->val - c2->val))->canonize();
            }
        }

        if(n0 != nullptr && n1 != nullptr && n0->type == OADD && n1->type == OADD) {
            NodeConstant *c1, *c2;
            if(n0->parameters.size() == 2 && n1->parameters.size() == 2 &&
               (c1 = dynamic_cast<NodeConstant *>(n0->parameters[1])) != nullptr &&
               (c2 = dynamic_cast<NodeConstant *>(n1->parameters[1])) != nullptr) {
                c1->val = c1->val - c2->val;
                newParams[1] = n1->parameters[0];
                return (createNodeOperator(operatorToString(newType)))->addParameters(newParams)->canonize();
                //((XNodeLeaf< ? > )
                //ns1[1]).value = (long) ns1[1].firstVal() - ns2[1].firstVal();
                //newParams[1] = ns2[0];
                //return new XNodeParent<V>(newType, newParams).canonization();
            }
        }

    }
    return (createNodeOperator(operatorToString(newType)))->addParameters(newParams);
}

