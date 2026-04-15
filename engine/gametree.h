#pragma once
/**
 * @file gametree.h
 * @brief Game Tree Data Structure for Educational & Analysis Purposes
 * 
 * This module provides explicit game tree representation and analysis tools.
 * While the chess engine uses implicit trees (for efficiency), this structure
 * is useful for:
 * - Understanding search tree structure
 * - Analyzing move ordering effectiveness
 * - Visualizing algorithm behavior
 * - Educational demonstrations
 * 
 * Time Complexity Analysis:
 * - Build tree to depth d: O(b^d) where b = branching factor
 * - Perfect move ordering: O(b^(d/2)) nodes
 * - With alpha-beta pruning: typically O(b^(3d/4))
 */

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <algorithm>

/**
 * NodeType - Categorizes nodes for pruning analysis
 */
enum class NodeType {
    MAX_NODE,        ///< Maximizing node (AI trying to maximize score)
    MIN_NODE,        ///< Minimizing node (opponent trying to minimize score)
    LEAF_NODE        ///< Terminal node (depth reached or game over)
};

/**
 * PruneType - Tracks why a node was pruned or explored
 */
enum class PruneType {
    NO_PRUNE,        ///< All children were explored
    ALPHA_PRUNE,     ///< Alpha cutoff (maximizer's best option elsewhere)
    BETA_PRUNE,      ///< Beta cutoff (minimizer's best option elsewhere)
    LEAF_PRUNE       ///< Not explored - leaf node
};

/**
 * GameTreeNode - Represents a single node in the game tree
 * 
 * Data Structure: Tree with parent pointers (for path reconstruction)
 * Memory per node: ~200 bytes
 * 
 * Invariants:
 * - Parent pointer forms valid tree structure (no cycles)
 * - Score is valid only if children_evaluated == true
 * - Alpha/Beta bounds are monotonically maintained along path
 */
struct GameTreeNode {
    // Tree structure
    std::shared_ptr<GameTreeNode> parent;
    std::vector<std::shared_ptr<GameTreeNode>> children;
    
    // Position information
    std::string move;              ///< Move from parent (e.g., "e2e4")
    int depth;                     ///< Distance from root (0-indexed)
    NodeType node_type;            ///< MAX or MIN node
    
    // Evaluation
    int score = 0;                 ///< Minimax score of this position
    int alpha = -100000;           ///< Alpha at time of evaluation
    int beta = 100000;             ///< Beta at time of evaluation
    
    // Analysis tracking
    uint64_t nodes_in_subtree = 0; ///< Total nodes evaluated below this
    PruneType prune_type = PruneType::NO_PRUNE;
    bool children_evaluated = false;
    
    /// Construct a game tree node
    GameTreeNode(int d, NodeType type, const std::string& m = "root")
        : depth(d), node_type(type), move(m) {}
    
    /**
     * Add a child node
     * Time: O(1) amortized
     * @return Shared pointer to new child
     */
    std::shared_ptr<GameTreeNode> addChild(const std::string& move_str) {
        NodeType child_type = (node_type == NodeType::MAX_NODE) 
                             ? NodeType::MIN_NODE 
                             : NodeType::MAX_NODE;
        auto child = std::make_shared<GameTreeNode>(depth + 1, child_type, move_str);
        child->parent = shared_from_this();
        children.push_back(child);
        return child;
    }
    
    /**
     * Get path from root to this node
     * Time: O(d) where d = depth
     * Space: O(d)
     * @return Vector of moves from root to this node
     */
    std::vector<std::string> getPath() const {
        std::vector<std::string> path;
        auto current = std::make_shared<GameTreeNode>(*this);
        while(current->parent) {
            path.push_back(current->move);
            current = current->parent;
        }
        std::reverse(path.begin(), path.end());
        return path;
    }
};

/**
 * GameTree - Complete game tree with analysis utilities
 * 
 * Design Pattern: Composite pattern (tree of nodes)
 * Access Pattern: Depth-first (recursive minimax)
 * 
 * Typical Size (depth 8, b=35):
 * - Nodes: ~2 billion without pruning
 * - Memory: Would need ~400 GB
 * - With alpha-beta: ~1.5 million nodes, ~300 MB (practical)
 */
class GameTree {
private:
    std::shared_ptr<GameTreeNode> root;
    uint64_t total_nodes = 0;        ///< Total nodes created
    uint64_t pruned_nodes = 0;       ///< Nodes skipped by pruning
    std::unordered_map<std::string, int> pruning_stats; ///< Type counts
    
public:
    GameTree() : root(std::make_shared<GameTreeNode>(0, NodeType::MAX_NODE)) {}
    
    /// Get root of tree
    std::shared_ptr<GameTreeNode> getRoot() const { return root; }
    
    /// Total nodes in tree
    uint64_t getTotalNodes() const { return total_nodes; }
    
    /// Nodes eliminated by pruning
    uint64_t getPrunedNodes() const { return pruned_nodes; }
    
    /**
     * Calculate branching statistics
     * Time: O(n) where n = total_nodes
     * @return Average branching factor
     */
    double getAverageBranchingFactor() {
        if(total_nodes <= 1) return 0;
        uint64_t internal_nodes = 0;
        uint64_t total_children = 0;
        
        traverseDepthFirst(root, [&](const std::shared_ptr<GameTreeNode>& node) {
            if(!node->children.empty()) {
                internal_nodes++;
                total_children += node->children.size();
            }
        });
        
        return internal_nodes > 0 ? (double)total_children / internal_nodes : 0;
    }
    
    /**
     * Count nodes by pruning type
     * Time: O(n)
     * @return Map of prune types to counts
     */
    std::unordered_map<std::string, int> getPruningStatistics() {
        std::unordered_map<std::string, int> stats;
        
        traverseDepthFirst(root, [&](const std::shared_ptr<GameTreeNode>& node) {
            switch(node->prune_type) {
                case PruneType::NO_PRUNE:
                    stats["explored"]++;
                    break;
                case PruneType::ALPHA_PRUNE:
                    stats["alpha_pruned"]++;
                    pruned_nodes++;
                    break;
                case PruneType::BETA_PRUNE:
                    stats["beta_pruned"]++;
                    pruned_nodes++;
                    break;
                case PruneType::LEAF_PRUNE:
                    stats["leaf_nodes"]++;
                    break;
            }
        });
        
        return stats;
    }
    
    /**
     * Find nodes at specific depth (for analysis)
     * Time: O(n)
     * @param target_depth Depth to search for
     * @return Vector of nodes at that depth
     */
    std::vector<std::shared_ptr<GameTreeNode>> getNodesAtDepth(int target_depth) {
        std::vector<std::shared_ptr<GameTreeNode>> result;
        
        traverseDepthFirst(root, [&](const std::shared_ptr<GameTreeNode>& node) {
            if(node->depth == target_depth) {
                result.push_back(node);
            }
        });
        
        return result;
    }
    
    /**
     * Calculate tree height
     * Time: O(n) worst case (unbalanced tree)
     * @return Maximum depth from root
     */
    int getHeight() {
        int max_depth = 0;
        traverseDepthFirst(root, [&](const std::shared_ptr<GameTreeNode>& node) {
            max_depth = std::max(max_depth, node->depth);
        });
        return max_depth;
    }
    
    /**
     * Depth-first recursive traversal
     * Time: O(n) where n = nodes visited
     * Space: O(h) where h = height (recursion stack)
     */
    template<typename Func>
    void traverseDepthFirst(const std::shared_ptr<GameTreeNode>& node,
                           Func callback) {
        if(!node) return;
        
        callback(node);
        total_nodes++;
        
        for(const auto& child : node->children) {
            traverseDepthFirst(child, callback);
        }
    }
    
    /**
     * Breadth-first traversal (level-order)
     * Time: O(n)
     * Space: O(w) where w = maximum width at any level
     */
    template<typename Func>
    void traverseBreadthFirst(Func callback) {
        if(!root) return;
        
        std::vector<std::shared_ptr<GameTreeNode>> current_level = {root};
        
        while(!current_level.empty()) {
            std::vector<std::shared_ptr<GameTreeNode>> next_level;
            
            for(const auto& node : current_level) {
                callback(node);
                for(const auto& child : node->children) {
                    next_level.push_back(child);
                }
            }
            
            current_level = next_level;
        }
    }
    
    /**
     * Generate tree visualization as ASCII tree (for debugging)
     * Time: O(n)
     * @return String representation of tree
     */
    std::string toASCIITree() {
        std::string result;
        
        traverseDepthFirst(root, [&](const std::shared_ptr<GameTreeNode>& node) {
            // Indentation based on depth
            for(int i = 0; i < node->depth; i++) {
                result += "  ";
            }
            
            result += (node->node_type == NodeType::MAX_NODE ? "▲" : "▼");
            result += " " + node->move;
            result += " (score=" + std::to_string(node->score) + ")";
            
            if(node->prune_type != PruneType::NO_PRUNE) {
                result += " [PRUNED]";
            }
            
            result += "\n";
        });
        
        return result;
    }
};

/**
 * COMPLEXITY ANALYSIS SUMMARY
 * 
 * Game Tree Construction:
 * - Without pruning: O(b^d) time, O(b^d) space
 * - With pruning: O(b^(3d/4)) time, O(b^(3d/4)) space
 * 
 * Traversal Operations:
 * - DFS: O(n) time, O(h) space
 * - BFS: O(n) time, O(w) space
 * - Path reconstruction: O(d) time
 * 
 * Real-world example (chess, depth 10, b=35):
 * - Unpruned tree would need ~2.76 × 10^15 nodes
 * - With alpha-beta pruning: ~100 million nodes (manageable)
 * - Memory: ~20 GB (still large, but modern systems can handle)
 * 
 */
