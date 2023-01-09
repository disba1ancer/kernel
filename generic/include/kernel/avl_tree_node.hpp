#ifndef AVL_TREE_NODE_H
#define AVL_TREE_NODE_H

#include <type_traits>

namespace kernel::intrusive {

template <typename Tag = void>
struct AVLTreeNode;

template <>
struct AVLTreeNode<void> {
    AVLTreeNode* parent;
    AVLTreeNode* children[2];
    int balance;
    AVLTreeNode() {};
};

template <typename Tag>
struct AVLTreeNode : AVLTreeNode<> {};

template <typename T>
struct AVLTreeNodeTraits;

template <typename T>
struct AVLTreeNodeTraits<AVLTreeNode<T>> {
    static auto GetParent(AVLTreeNode<T>& node) -> AVLTreeNode<T>*
    {
        return node.parent;
    }
    static void SetParent(AVLTreeNode<T>& node, AVLTreeNode<T>* parent)
    {
        node.parent = parent;
    }
    static auto GetChild(AVLTreeNode<T>& node, bool right) -> AVLTreeNode<T>*
    {
        return node.children[right];
    }
    static void SetChild(AVLTreeNode<T>& node, bool right, AVLTreeNode<T>* child)
    {
        node.children[right] = child;
    }
    static  int GetBalance(AVLTreeNode<T>& node)
    {
        return node.balance;
    }
    static void SetBalance(AVLTreeNode<T>& node, int balance)
    {
        node.balance = balance;
    }
};

namespace avl_tree_detail {

struct MoveConstructible {
    MoveConstructible() = default;
    MoveConstructible(const MoveConstructible&) = delete;
    MoveConstructible(MoveConstructible&&) = default;
    MoveConstructible& operator=(const MoveConstructible&) = delete;
    MoveConstructible& operator=(MoveConstructible&&)& = delete;
};

template <typename Drvd, typename T>
struct Comparable : MoveConstructible {
    using ValueType = T;
    operator ValueType() const
    {
        return +*Get();
    }
//    friend bool operator==(const Comparable& one, const Comparable& oth)
//    {
//        return +*one.Get() == +*oth.Get();
//    }
//    friend bool operator==(const Comparable& one, const ValueType& oth)
//    {
//        return +*one.Get() == oth;
//    }
private:
    Drvd* Get()
    {
        return static_cast<Drvd*>(this);
    }
    auto Get() const -> const Drvd*
    {
        return static_cast<const Drvd*>(this);
    }
};

template <typename T>
struct ChildrenHelper;

template <typename T>
struct ParentHelper;

template <typename T>
struct BalanceHelper : Comparable<BalanceHelper<T>, int> {
    using NodeType = T;
    using NodeTraits = AVLTreeNodeTraits<NodeType>;
    BalanceHelper(NodeType* node) : node(node)
    {}
    auto operator=(int newBalance) -> int
    {
        NodeTraits::SetBalance(*node, newBalance);
        return newBalance;
    }
    auto operator=(const BalanceHelper& hlp) -> BalanceHelper&
    {
        *this = +hlp;
        return *this;
    }
    auto operator+() const -> int
    {
        return NodeTraits::GetBalance(*node);
    }
private:
    NodeType* node;
};

template <typename T>
struct RecoursiveHelper;

template <template <typename> typename P, typename N>
struct RecoursiveHelper<P<N>> {
    using Derived = P<N>;
    using NodeType = N;
    auto Children(bool right) -> ChildrenHelper<NodeType>
    {
        return { (NodeType*)(*Get()), right };
    }
    auto Balance() -> BalanceHelper<NodeType>
    {
        return { (NodeType*)(*Get()) };
    }
    auto Parent() -> ParentHelper<NodeType>
    {
        return { (NodeType*)(*Get()) };
    }
private:
    Derived* Get()
    {
        return static_cast<Derived*>(this);
    }
};

template <typename T>
struct ChildrenHelper :
    RecoursiveHelper<ChildrenHelper<T>>,
    Comparable<ChildrenHelper<T>, T*>
{
    using NodeType = T;
    using NodeTraits = AVLTreeNodeTraits<NodeType>;
    ChildrenHelper(NodeType* node, bool right) :
        node(node),
        right(right)
    {}
    auto operator=(NodeType* newNode) -> NodeType*
    {
        NodeTraits::SetChild(*node, right, newNode);
        return newNode;
    }
    auto operator=(const ChildrenHelper& hlp) -> ChildrenHelper&
    {
        *this = +hlp;
        return *this;
    }
    auto operator+() const -> NodeType*
    {
        return NodeTraits::GetChild(*node, right);
    }
private:
    NodeType* node;
    bool right;
};

template <typename T>
struct ParentHelper :
    RecoursiveHelper<ParentHelper<T>>,
    Comparable<ParentHelper<T>, T*>
{
public:
    using NodeType = T;
    using NodeTraits = AVLTreeNodeTraits<NodeType>;
    ParentHelper(NodeType* node) :
        node(node)
    {}
    auto operator=(NodeType* newNode) -> NodeType*
    {
        NodeTraits::SetParent(*node, newNode);
        return newNode;
    }
    auto operator=(const ParentHelper& hlp) -> ParentHelper&
    {
        *this = +hlp;
        return *this;
    }
    auto operator+() const -> NodeType*
    {
        return NodeTraits::GetParent(*node);
    }
private:
    NodeType* node;
};

}

template <typename T>
struct AVLTreeNodeTraitsHelper :
    avl_tree_detail::RecoursiveHelper<AVLTreeNodeTraitsHelper<T>>,
    avl_tree_detail::Comparable<AVLTreeNodeTraitsHelper<T>, T*>
{
public:
    using NodeType = T;
    AVLTreeNodeTraitsHelper(NodeType* node) : node(node)
    {}
    auto operator=(NodeType* newNode) -> NodeType*
    {
        return node = newNode;
    }
    auto operator+() const -> NodeType*
    {
        return node;
    }
private:
    NodeType* node;
};

}

#endif // AVL_TREE_NODE_H
