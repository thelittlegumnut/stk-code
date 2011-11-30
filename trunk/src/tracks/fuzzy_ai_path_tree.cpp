//  $Id: fuzzy_ai_path_tree.cpp 10225 2011-11-26 12:31:33Z Kinsu $
//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2006 SuperTuxKart-Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#define AI_DEBUG

#include <iostream>

#include "tracks/fuzzy_ai_path_tree.hpp"
#include "tracks/quad_graph.hpp"
#include "items/item_manager.hpp"

using namespace std;

// TODO : make comments doxygen compliant

//==============================================================================
// Constructor & destructor related functions

//------------------------------------------------------------------------------
/** Constructor
 *  Initializes a tree of paths based on a driveline fork. The constructor first
 *  calls the buildTree function to create the tree architecture, and then the
 *  setPathData function to fill in the path data.
 */
FuzzyAiPathTree::FuzzyAiPathTree(unsigned int rootNodeId = 0)
{
    assert(QuadGraph::get()); // Cannot build tree if QuadGraph does not exist

#ifdef AI_DEBUG
    printPossiblePaths();
#endif

    m_treeRoot = buildTree(rootNodeId);
//    print(); // debug
    m_treeRoot = setPathData(m_treeRoot);
    m_compareData = setComparableData(m_treeRoot);


// Debug output
//    cout << "COMPARE DATA is AFTER BUILDING : " << endl;
//    for(unsigned int i=0; i<m_compareData->size() ; i++)
//    {
//        cout << "vector " << i << ":" << endl;
//        for(unsigned int j=0; j<m_compareData->at(i)->size() ; j++)
//        {
//            cout << "pathData " << j << " = ";
//            cout << m_compareData->at(i)->at(j)->pathLength << endl;
//        }
//    }
} // Constructor

//------------------------------------------------------------------------------
/** The buildTree functions recursively builds a tree structure using TreeNode
 *  instances. Each node of a tree corresponds to a path fork, and will later
 *  store data about the road section that leads to the fork (see setPathData).
 *  Each leave corresponds to a path end.
 *  An example of multi-level tree, and the corresponding driveline :
 *
 *            _____1_______             |                    Fork1
 *   -> _____/__0____0.0___\_____       |                   /    \
 *          ^    ^\__0.1__/^^           |               Fork2    Path1 End
 *          |    |         |P1,P0.0 End |              /    \
 *     Fork 1   Fork2    Path0.1 End    |    Path0.0 End    Path0.1 End
 */
TreeNode* FuzzyAiPathTree::buildTree(unsigned int startNodeId)
{
//    cout << "Building tree... begin : " << startNodeId << endl;
    unsigned int         curNodeId = startNodeId;
    TreeNode*            rootNode = new TreeNode(startNodeId, NULL, NULL);
    vector<unsigned int> nextGraphNodes(1, curNodeId); // Init to enter loop

//    cout << "Current node : ";
    // While there is no fork, and no path change (eg. an alt. path that merges
    //  back with the main path), go forward.
    while(nextGraphNodes.size() == 1 && nextGraphNodes[0] == curNodeId)
    {
//        cout << curNodeId << ", ";
        nextGraphNodes.clear();
        QuadGraph::get()->getSuccessors(curNodeId, nextGraphNodes, true);
        curNodeId ++;
    }
//    cout << "... stopped loop because : ";
//    if(nextGraphNodes[0] != curNodeId)
//        cout << "unexpected nodeID, current path has changed. Expected = ";
//        cout << curNodeId << ", Got = " << nextGraphNodes[0] << endl;
    
    rootNode->nodeId = curNodeId - 1;

    if(nextGraphNodes.size() > 1) // if there is a fork, build sub-trees
    {
//        cout << "fork detected, successor count = " << nextGraphNodes.size();
//        cout << endl << "Creating fork-based path trees ...";
        rootNode->children = new vector<TreeNode*>();
        for(unsigned int i=0 ; i<nextGraphNodes.size() ; i++)
        {
//            cout << "choice " << i << ", node " << nextGraphNodes[i] << ", ";
            rootNode->children->push_back(buildTree(nextGraphNodes[i]));
        }
//        cout << "end of tree creation" << endl;
    }
//    cout << "build tree node end..." << endl;
    
    return rootNode;
} // buildTree

//------------------------------------------------------------------------------
/** This function is used to compute and store path data in the tree nodes, once
 *  the tree has been built (by the buildTree function).
 *
 *  It first updates the tree, making sure that every path stored in it has the
 *  same starting quadGraph node and the same ending quadGraph node. This is
 *  useful in some complex cases, when the track has a 2nd fork after the 1st
 *  fork, which creates a branch that ends after the other 1st fork branch, on
 *  the main path (see below).
 *
 *              ___0.1___           alt. path 0.1 (left at 2nd fork)
 *  -> _____0__/___0.0___\_____  main path (always 0)
 *        \___1___/                 alt. path 1 (right at 1st fork)
 *
 *  To compare a path 0.x with the path 1, the end point of the path 1 must be
 *  set to the point where the path 0.1 joins back the main path. So the path 1
 *  is "rallongé" (todo translate).
 *
 *  When all the paths have the same ending point, data about each path is
 *  recursively computed and added to the nodes (length, bonus & malus count).
 *  After this, the tree corresponding to the below track will be :
 *  
 *           ___x__o__x___      Path1   |               Root (node=42,
 *   -> ____/___x_____o___\____Path0    |             /  |  \   data=NULL)
 *         ^\___o__/^      ^    Path2   |           /    |    \
 *         |        |      |            |      Path1   Path0   Path2
 *  Node:  42      66     84            |   node=84   node=84   node=84
 *                             o: box   |   o=1,x=2   o=1,x=1   o=2,x=0
 *                          x: banana   |   len=17    len=15    len=17
 */
TreeNode* FuzzyAiPathTree::setPathData(TreeNode* rootNode, PathData* rootData)
{
    unsigned int rootNodeId = rootNode->nodeId;
    unsigned int treeEndNodeId = getFarthestNode(rootNode);

    if(rootData)       // If the rootData parameter exists, set this node's data
        rootNode->data = rootData;

    // If the current node has children
    if(rootNode->children)
    {
        unsigned int          branchEndNodeId;
        TreeNode*             curChild;
        vector<unsigned int>  firstPathNodes;

        QuadGraph::get()->getSuccessors(rootNodeId, firstPathNodes, true);
#ifdef AI_DEBUG
        assert(firstPathNodes.size() != 1); // Cannot be 1 (see buildTree)
#endif
        // Get the children paths
        for(unsigned int i=0; i<rootNode->children->size() ; i++)
        {
            unsigned int          curNodeId = firstPathNodes[i];
            unsigned int          lastNodeId = rootNodeId;
            PathData*             childData = new PathData();
            vector<unsigned int>  nextGraphNodes;
            
            curChild = rootNode->children->at(i);
            // If the current node is a leave, set its node to the "tree end"
            if(curChild->children == NULL)
                curChild->nodeId = treeEndNodeId;
            
            branchEndNodeId = curChild->nodeId;
            
            // Follow the path until its end, gather and store path data            
            while(curNodeId != branchEndNodeId)
            {
                nextGraphNodes.clear();
                QuadGraph::get()->getSuccessors(curNodeId, nextGraphNodes,true);
#ifdef AI_DEBUG
                assert(nextGraphNodes.size() == 1);//Can only be 1,see buildTree
#endif
                const Quad q = QuadGraph::get()->getQuadOfNode(curNodeId);
                const Quad q2 = QuadGraph::get()->getQuadOfNode(lastNodeId); 
                float dist = (q.getCenter() - q2.getCenter()).length();
                childData->bonusCount += item_manager->getQuadBonusCount(q);
                childData->malusCount += item_manager->getQuadMalusCount(q);
                childData->pathLength += dist;
                
                lastNodeId = curNodeId;
                curNodeId = nextGraphNodes[0];
            } // while branch end has not been reached yet
            
            setPathData(curChild, childData);
        } // for each child
    } // if node has children
    
    return rootNode;
} // setPathData

//------------------------------------------------------------------------------
/** The getFarthestNode function is used to retrieve the first main-driveline
 *  node that can be reached after taking any of the paths in the tree, i.e. the
 *  node that can be considered as the merging point of all these paths.
 *  The last quad of the main path must not be a divergent fork for this
 *  function to work well (not so sure about that...).
 *  This function must be used on a tree from the buildTree function, and is
 *  called by the setPathData function.
 */
unsigned int FuzzyAiPathTree::getFarthestNode(const TreeNode* rootNode) const
{
    vector<unsigned int> branchLastNodes = vector<unsigned int>();

    if(!rootNode->children)
    {
        vector<unsigned int> nextGraphNodes;
        unsigned int curNodeId = rootNode->nodeId;
        unsigned int mainPathLength = QuadGraph::get()->getLapQuadCount();
        // Don't take in account the nodes which id > last node of the main path
        if(curNodeId <= mainPathLength - 1)
        {
            QuadGraph::get()->getSuccessors(curNodeId, nextGraphNodes, true);
#ifdef AI_DEBUG
            assert(nextGraphNodes.size() == 1); // Can only be 1 (see buildTree)
#endif
            branchLastNodes.push_back(nextGraphNodes[0]);
        } // If current node is the last main driveline node, or higher
        else
        {
            branchLastNodes.push_back(0);
        }
    } // If current node has no children
    else
    {
        for(unsigned int i=0; i<rootNode->children->size(); i++)
        {
            TreeNode* currentChild = rootNode->children->at(i);
            branchLastNodes.push_back(getFarthestNode(currentChild));
        }
    }
    return *max_element(branchLastNodes.begin(), branchLastNodes.end());
} // getFarthestNode

//------------------------------------------------------------------------------
/** This function builds and returns a vector of PathData* vectors. Each
 *  PathData instance gives information about a specific possible path that can
 *  be taken by a vehicle when it is on the tree root (fork quadGraph node).
 * 
 *  The returned vector contains a PathData* vector for each choice one has
 *  when being at the rootNode.
 *                        For instance, if the fork lets 3 choices, it will
 *        ___1___         contain 3 vectors. The first of these 3 vectors will
 * -> ___/___0___\___->   be the vector of the possible paths if the taken
 *       \___2___/        choice is to follow the first successor (i.e. the
 *                        successor 0). In this case, the returned vector is
 *  [sub-vector 0, sub-vector 1, sub-vector 2]
 *  Each one of the sub-vectors contains one pathData*, as there is only one
 *  path to follow once the choice has been taken (go left, right or straight).
 *
 *  Most of the time, the sub-vectors will only hold 1 pathData*. To support
 *  more complex tracks, it can store data for several paths, that would be the
 *  result of a track having other forks on one of the paths after the 1st fork.
 *                              In such a case, the returned vector would
 *         ___1________         contain 2 sub-vectors : one for the path which
 *  -> ___/_0___0.0____\___->   begins at the 1st fork successor, and another
 *            \__0.1___/        for the path which begins at the 2nd.
 *              \_0.2_/         The first sub-vector will hold 3 PathData* :
 *                              one for the path 0.0, one for the path 0.1,
 *  and another for the path 0.2.
 *  The AI will then be able to compare these 3 paths with the only PathData*
 *  stored in the 2nd sub-vector (path 1), to take the decision (right or left)
 */
vector<vector<PathData*>*> *FuzzyAiPathTree::setComparableData
                                                          (const TreeNode* root)
{
    vector<vector<PathData*>*> *data = new vector<vector<PathData*>*>();
//    cout << "setComparableData debug :";
    
    if(root->children)
    {
//        cout << "has Children : " << root->nodeId << endl;
        vector<vector<PathData*>*> *childData;
        for(unsigned int i=0 ; i<root->children->size() ; i++)
        {
//            cout<<"child "<<i<<", node "<<root->children->at(i)->nodeId<<endl; 
            childData = setComparableData(root->children->at(i));
            for(unsigned int j=0 ; j<childData->size() ; j++)
            {
//                cout << endl << "\tvector " << j << " : " << endl;
                vector<PathData*>* childCurChoiceData = new vector<PathData*>();
                for(unsigned int k=0 ; k<childData->at(j)->size() ; k++)
                {
//                    cout << "\t\t pathData " << k;
                    PathData* curData = childData->at(j)->at(k);
                    PathData* rootData = root->data;

                    if(rootData)
                    {
//                        cout<<" : rootHasData (" << rootData->pathLength;
//                        cout<<")adding childData "<<curData->pathLength<<endl;
                        rootData = new PathData(root->data->pathLength,
                                                root->data->bonusCount,
                                                root->data->malusCount);
                        sumPathData(rootData, rootData, curData);
                    } // if rootData is not NULL
                    else
                    {
//                        cout<<" : no data in root...just copying childData (";
//                        cout << curData->pathLength << endl;
                        rootData = new PathData(curData->pathLength,
                                                curData->bonusCount,
                                                curData->malusCount);
                    } // if rootData is NULL
                    
//                    cout << "\t\t ... adding data to choice vector";
                    childCurChoiceData->push_back(rootData);

                } // for each data object in the current child data vector
                
//                cout << "... adding choice vector to global vector" << endl;
                data->push_back(childCurChoiceData);
            } // for each children data vector
        } // for each children
    } // if has children
    else  // If this is a leave, just add a single data vector to the big vector
    {
//        cout << "no children.. just copying : " << root->nodeId << endl;

        vector<PathData*> *currentData = new vector<PathData*>();
        currentData->push_back(new PathData(root->data->pathLength,
                                            root->data->bonusCount,
                                            root->data->malusCount));
        data->push_back(currentData);
    } // if does not have any children
//    cout << "Function end" << endl;
    return data;
    
} // setComparableData

//------------------------------------------------------------------------------
/** Destructor
 *  Releases the allocated memory for the whole tree and the m_compareData
 *  vectors. */
FuzzyAiPathTree::~FuzzyAiPathTree()
{
    int i = m_compareData->size()-1; // int because with unsigned ints, 0-1 > 0
    for( ; i >= 0 ; i--)
    {
        int j = m_compareData->at(i)->size()-1 ; 
        for( ; j >= 0 ; j--)
            delete m_compareData->at(i)->at(j);
        delete m_compareData->at(i);
    }
    delete m_compareData;
    
    deleteTree(m_treeRoot);
} // ~FuzzyAiPathTree

// -- Recursive delete function called by the destructor --
void FuzzyAiPathTree::deleteTree(TreeNode* rootNode)
{
    if(rootNode->children)
    {
        int i = rootNode->children->size()-1;
        for(; i >= 0 ; i--)
            deleteTree(rootNode->children->at(i));
        delete rootNode->children;
    }
    
    if(rootNode->data)
        delete rootNode->data;
    
    delete rootNode;
} // deleteTree

// End of constructor & destructor related functions
//==============================================================================

//==============================================================================
// Debug functions
//------------------------------------------------------------------------------
/** Print tree
 */
void FuzzyAiPathTree::printNode(const TreeNode* rootNode) const
{
    if(rootNode)
    {
        cout << "[node " << rootNode->nodeId << ", ";
        if(rootNode->data)
        {
            cout << "b" << rootNode->data->bonusCount << ", m";
            cout << rootNode->data->malusCount << ", L";
            cout << rootNode->data->pathLength << " ";
        }
        if(rootNode->children != NULL)
        {
            for(unsigned int i=0; i<rootNode->children->size() ; i++)
                printNode(rootNode->children->at(i));
        }
        cout << "]";
    } // if(rootNode)
} // printNode

/** -- Print tree initialisation --
 */
void FuzzyAiPathTree::print() const
{
    printNode(m_treeRoot);
    cout << endl;
} // print

//------------------------------------------------------------------------------
/** 
 *  
 */
#ifdef AI_DEBUG
struct FuzzyAiPath
{
    vector<unsigned int>    *node_indexes;
    bool                    discovered;
    unsigned int            bonus_count;
    unsigned int            malus_count;
//        std::vector<FuzzyAiSubPath> subpaths;
    // TODO turn count, zipper_count ?
    
    FuzzyAiPath(std::vector<unsigned int> *n_indexes,
                    bool has_been_discovered, unsigned int bonus_count,
                    unsigned int malus_count) :
            node_indexes(n_indexes),
            discovered(false),
            bonus_count(0),
            malus_count(0)
        {}
};

void FuzzyAiPathTree::printPossiblePaths()
{
   /** This structure extends the quadgraph to store data about paths (eg.
     *  bonus count). This data is used by the fuzzy ai controller to choose
     *  which path to take. */


    vector<FuzzyAiPath *> possiblePaths = vector<FuzzyAiPath*>();
    
    vector<unsigned int> next;
    
    // Init main path
    vector<unsigned int> mainPathNodes = vector<unsigned int>();
    mainPathNodes.push_back(0); // begins with the 0 indexed node
    FuzzyAiPath newPath(&mainPathNodes, false, 0, 0);
    possiblePaths.push_back(&newPath);
    
    // Discover the paths in the path list (the main path first, alt.paths then)
    for(unsigned int i=0 ; i < possiblePaths.size() ; i++)
    {
        // Don't discover already discovered paths
        if(possiblePaths.at(i)->discovered)
            continue;
        
        vector<unsigned int> *currentPathNodes = possiblePaths.at(i)->node_indexes;
        unsigned int last_node_index = currentPathNodes->back();
        unsigned int next_node_index;
        
        do  // Follow the current path until its end & detect forks (=new paths)
        {
            next_node_index = 0;
            next.clear();
            
            // Get the successors of the last known node of the current path
            QuadGraph::get()->getSuccessors(last_node_index, next, true);
            
            if(next.size() > 1)      // In case of fork (ie. several successors)
            {
                // Create an undiscovered path in the list for each successor...
                for(unsigned int k=0 ; k < next.size() ; k++)
                {
                    // ...except for the successor of the current path
                    if(next[k] == last_node_index+1)
                    {
                        next_node_index = k;
                        continue;
                    }
                    
                    // Add the detected (but undiscovered yet) path to the list
                    vector<unsigned int> newPathNodes = vector<unsigned int>();
                    newPathNodes.push_back(last_node_index); // 1st node (fork)
                    newPathNodes.push_back(next[k]);         // 2nd node
                    FuzzyAiPath newPath(&newPathNodes, false, 0, 0);
                    possiblePaths.push_back(&newPath);
                } // Add the newly detected path(s) to the path list
            } // If the current node is a path fork
            
            // Append the node to the current path, and go to the next node
            currentPathNodes->push_back(next[next_node_index]);            
            last_node_index++;

        // Stop when the next node index is not the current node index + 1
        } while(next[next_node_index] == last_node_index);

        possiblePaths.at(i)->discovered = true;
    } // For each entry in the path list, discover the path

    // Set item count
    for(unsigned int i=0 ; i<possiblePaths.size() ; i++)
    {
        FuzzyAiPath *currentPath = possiblePaths.at(i);
        for(unsigned int j=0 ; j<currentPath->node_indexes->size() ; j++)
        {
            const Quad& q=QuadGraph::get()->getQuadOfNode(
                                              currentPath->node_indexes->at(j));
            currentPath->bonus_count += item_manager->getQuadBonusCount(q);
            currentPath->malus_count += item_manager->getQuadMalusCount(q);
        }
    }
    
    // Print everything
    cout << "Found paths : " << endl;
    for(unsigned int i2=0 ; i2 < possiblePaths.size() ; i2++)
    {
        cout << "\t path " << i2;
        cout << " : bonus_count = " << possiblePaths.at(i2)->bonus_count;
        cout << ", malus_count = " << possiblePaths.at(i2)->malus_count << endl;
        for(unsigned int j2=0; j2<possiblePaths.at(i2)->node_indexes->size(); j2++)
            cout << possiblePaths.at(i2)->node_indexes->at(j2) << ", ";
        
        cout << endl;
    }
}

#endif

// End of debug functions
//==============================================================================

//------------------------------------------------------------------------------
/** Sums the data1 and data2 attributes to the result parameter.
 */
void FuzzyAiPathTree::sumPathData( PathData* result, const PathData* data1,
                                                     const PathData* data2 )
{
    result->bonusCount = data1->bonusCount + data2->bonusCount;
    result->malusCount = data1->malusCount + data2->malusCount;
    result->pathLength = data1->pathLength + data2->pathLength;
} // sumPathData
