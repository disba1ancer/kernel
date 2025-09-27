#ifndef AVL_TREE_H
#define AVL_TREE_H

#include <cstddef>
#include "node.hpp"
#include "avl_tree_node.hpp"
#include <cstdlib>
#include <memory>

namespace kernel::intrusive {

namespace detail {

template <typename Comp, typename Key>
concept Comparator = requires (const Key t1, const Key t2, Comp c)
{
    { c(t1, t2) } -> std::same_as<bool>;
};

}

template <
    typename T,
    detail::Comparator<T> Comp,
        typename CastPolicyGen = BaseClassCastPolicy<AVLTreeNode<>, T>
>
class AVLTree : detail::ContainerNodeRequirments<T, CastPolicyGen> {
public:
    using CastPolicy = CastPolicyGen;
    using NodeType = typename CastPolicyGen::NodeType;
    using NodeTraits = AVLTreeNodeTraits<NodeType>;
    using TraitsHelper = AVLTreeNodeTraitsHelper<NodeType>;
    AVLTree()
    {
        using Tr = NodeTraits;
        Tr::SetParent(sentinel, nullptr);
        Tr::SetChild(sentinel, 0, std::addressof(sentinel));
        Tr::SetChild(sentinel, 1, std::addressof(sentinel));
    }

    class Iterator : public detail::BasicIterator<Iterator, T> {
        friend class AVLTree;
        Iterator(NodeType* current) noexcept :
            current(current)
        {}

        auto next(bool right) noexcept -> Iterator&
        {
            using Tr = NodeTraits;
            auto possibleNext = Tr::GetChild(*current, right);
            if (Tr::GetParent(*possibleNext) != current) {
                current = possibleNext;
                return *this;
            }
            current = FindNeighbour(current, right);
            return *this;
        }
    public:
        auto operator++() noexcept -> Iterator&
        {
            return next(true);
        }

        auto operator--() noexcept -> Iterator&
        {
            return next(false);
        }

        T* operator->() const noexcept
        {
            return CastPolicy::FromNode(current);
        }

        bool operator==(const Iterator& oth) const noexcept
        {
            return current == oth.current;
        }
    private:
        NodeType* current;
    };

    AVLTree(AVLTree&& oth) :
        sentinel(oth.sentinel)
    {
        using Tr = NodeTraits;
        auto first = oth.begin().current;
        Tr::SetChild(*first, 0, std::addressof(sentinel));
        auto last = (--oth.end()).current;
        Tr::SetChild(*last, 1, std::addressof(sentinel));
        auto root = Tr::GetChild(sentinel, 0);
        Tr::SetParent(*root, std::addressof(sentinel));
    }

    Iterator Insert(T& elem)
    {
        return InsertUnrestricted(UpperBound(elem), elem);
    }

    Iterator Insert(Iterator hint, T& elem)
    {
        Comp comp;
        if (!comp(elem, *hint) || comp(elem, *--hint)) {
            return Insert(elem);
        }
        return InsertUnrestricted(hint, elem);
    }

    Iterator Erase(Iterator it)
    {
        using Tr = AVLTreeNodeTraits<NodeType>;
        using H = TraitsHelper;
        auto erasedNode = H(it++.current);
        bool a = erasedNode.Children(0).Parent() == erasedNode;
        bool b = erasedNode.Children(1).Parent() == erasedNode;
        if (!(a && b)) {
            H children[] = {
                { erasedNode.Children(0) },
                { erasedNode.Children(1) }
            };
            bool c = erasedNode.Parent().Children(0) == erasedNode;
            if (a == b) {
                bool c = erasedNode.Children(1) != erasedNode.Parent();
                erasedNode.Parent().Children(c) = erasedNode.Children(c);
            } else {
                children[b].Parent() = erasedNode.Parent();
                children[b].Parent().Children(a) = children[b];
                children[b].Children(a) = children[a];
            }
            if (!a && erasedNode.Children(0) == std::addressof(sentinel)) {
                Tr::SetChild(sentinel, 1, erasedNode.Children(1));
            }
            RebalanceTreeE(erasedNode.Parent(), c);
            return it;
        }
        NodeType* neighbours[] = {
            FindNeighbour(erasedNode, false),
            it.current
        };
        bool c = erasedNode.Balance() <= 0;
        auto lowerNode = H(neighbours[c]);
        auto parent = H(lowerNode.Parent());
        H(neighbours[!c]).Children(c) = lowerNode;
        if (lowerNode.Children(c) != parent) {
            lowerNode.Children(c).Parent() = parent;
            parent.Children(!c) = lowerNode.Children(c);
        }
        bool d = erasedNode.Parent().Children(0) != erasedNode;
        lowerNode.Parent() = erasedNode.Parent();
        lowerNode.Parent().Children(d) = lowerNode;

        if (erasedNode.Children(0) != lowerNode) {
            lowerNode.Children(0) = erasedNode.Children(0);
            lowerNode.Children(0).Parent() = lowerNode;
        }

        if (erasedNode.Children(1) != lowerNode) {
            lowerNode.Children(1) = erasedNode.Children(1);
            lowerNode.Children(1).Parent() = lowerNode;
        }

        lowerNode.Balance() = erasedNode.Balance();

        RebalanceTreeE(parent, c);
        return it;
    }

    Iterator Erase(Iterator b, Iterator e)
    {
        EraseInternal(b, e);
        return e;
    }

    std::size_t Erase(T& elem)
    {
        Erase(IteratorTo(elem));
        return 1;
    }

    template <typename KeyType>
    std::size_t Erase(const KeyType& elem)
    {
        return EraseInternal(LowerBound(elem), UpperBound(elem));
    }

    Iterator Begin()
    {
        using Tr = AVLTreeNodeTraits<NodeType>;
        return Tr::GetChild(sentinel, 1);
    }

    friend Iterator begin(AVLTree& tree)
    {
        return tree.Begin();
    }

    Iterator End()
    {
        return std::addressof(sentinel);
    }

    friend Iterator end(AVLTree& tree)
    {
        return tree.End();
    }

    struct InsertResult {
        Iterator it;
        bool increased;
    };

    auto IteratorTo(T& elem) -> Iterator
    {
        using cp = CastPolicy;
        return cp::ToNode(std::addressof(elem));
    }

    auto Find(const T& key) -> Iterator {
        return Find<T>(key);
    }

    template <typename KeyType = T>
    requires(requires (const T& t, const KeyType& k, Comp comp) {
        comp(k, t);
        comp(t, k);
    })
    auto Find(const KeyType& key) -> Iterator
    {
        using Tr = AVLTreeNodeTraits<NodeType>;
        using H = TraitsHelper;
        using cp = CastPolicy;
        auto subtree = H(Tr::GetChild(sentinel, 0));
        if (subtree == std::addressof(sentinel)) {
            return std::addressof(sentinel);
        }
        NodeType* range[2] = { std::addressof(sentinel), std::addressof(sentinel) };
        while (true) {
            Comp comp;
            bool a = comp(key, *cp::FromNode(subtree));
            bool b = comp(*cp::FromNode(subtree), key);
            if (a == b) return { subtree };
            if (subtree.Children(b) == range[b]) {
                return std::addressof(sentinel);
            }
            range[a] = subtree;
            subtree = subtree.Children(b);
        }
        return std::addressof(sentinel);
    }

    template <typename KeyType>
    auto UpperBound(const KeyType& key) -> Iterator
    {
        using Tr = AVLTreeNodeTraits<NodeType>;
        using H = TraitsHelper;
        using cp = CastPolicy;
        auto subtree = H(Tr::GetChild(sentinel, 0));
        if (subtree == std::addressof(sentinel)) {
            return std::addressof(sentinel);
        }
        NodeType* range[2] = { std::addressof(sentinel), std::addressof(sentinel) };
        while (true) {
            Comp comp;
            bool a = comp(key, *cp::FromNode(subtree));
            if (subtree.Children(!a) == range[!a]) {
                return a ? subtree : subtree.Children(1);
            }
            range[a] = subtree;
            subtree = subtree.Children(!a);
        }
        return std::addressof(sentinel);
    }

    template <typename KeyType>
    auto LowerBound(const KeyType& key) -> Iterator
    {
        using Tr = AVLTreeNodeTraits<NodeType>;
        using H = TraitsHelper;
        using cp = CastPolicy;
        auto subtree = H(Tr::GetChild(sentinel, 0));
        if (subtree == std::addressof(sentinel)) {
            return std::addressof(sentinel);
        }
        NodeType* range[2] = { std::addressof(sentinel), std::addressof(sentinel) };
        while (true) {
            Comp comp;
            bool a = comp(*cp::FromNode(subtree), key);
            if (subtree.Children(a) == range[a]) {
                return a ? subtree.Children(1) : subtree;
            }
            range[!a] = subtree;
            subtree = subtree.Children(a);
        }
        return std::addressof(sentinel);
    }

    bool Empty()
    {
        return Begin() == End();
    }
private:
    Iterator InsertUnrestricted(Iterator hint, T& elem)
    {
        using Tr = AVLTreeNodeTraits<NodeType>;
        using H = TraitsHelper;
        using cp = CastPolicy;
        auto parentNode = H(hint.current);
        bool rightInsert = parentNode.Children(0).Parent() == parentNode;
        if (rightInsert) {
            parentNode = FindNeighbour(parentNode, false);
        }
        auto node = H(cp::ToNode(std::addressof(elem)));
        node.Balance() = 0;
        node.Parent() = parentNode;
        node.Children(!rightInsert) = parentNode;
        node.Children(rightInsert) = parentNode.Children(rightInsert);
        if (node.Children(0) == std::addressof(sentinel)) {
            Tr::SetChild(sentinel, 1, node);
        } // TODO: sentinel update without branch
        parentNode.Children(rightInsert) = node;
        RebalanceTreeI(parentNode, rightInsert);
        return { node };
    }

    void RebalanceTreeI(NodeType* fromArg, bool balanceSign)
    {
        using H = TraitsHelper;
        auto from = H(fromArg);
        while (from != std::addressof(sentinel)) {
            int balanceDiff = 1 - 2 * balanceSign;
            from.Balance() = from.Balance() + balanceDiff;
            auto nextNode = H(from.Parent());
            bool chInd = nextNode.Children(0) != from;
            if (
                from.Balance() == 0 ||
                (std::abs(from.Balance()) == 2 &&
                RotateSubtree(nextNode, chInd, from.Balance() > 0))
            ) {
                return;
            }
            balanceSign = chInd;
            from = +nextNode;
        }
    }

    std::size_t EraseInternal(Iterator b, Iterator e)
    {
        std::size_t count = 0;
        for (auto i = b; i != e; i = Erase(i)) { ++count; }
        return count;
    }

    void RebalanceTreeE(NodeType* fromArg, bool balanceSign)
    {
        using H = TraitsHelper;
        auto from = H(fromArg);
        while (from != std::addressof(sentinel)) {
            int balanceDiff = 1 - 2 * balanceSign;
            from.Balance() = from.Balance() + balanceDiff;
            auto nextNode = H(from.Parent());
            bool chInd = nextNode.Children(0) != from;
            if (
                from.Balance() != 0 &&
                std::abs(from.Balance()) == 2 &&
                !RotateSubtree(nextNode, chInd, from.Balance() > 0)
            ) {
                return;
            }
            balanceSign = !chInd;
            from = +nextNode;
        }
    }

    bool RotateSubtree(NodeType* parent, bool parentRight, bool right)
    {
        using H = TraitsHelper;
        auto subtree = H(parent).Children(parentRight);
        auto a = H(subtree), b = H(subtree.Children(!right));
        int dirSign = right * 2 - 1;
        if (b.Balance() * dirSign < 0) {
            subtree = RotateSubtreeBig(a, b, right);
            return true;
        }
        bool result = (b.Balance() != 0);
        if (b.Children(right) != a) {
            a.Children(!right) = b.Children(right);
            a.Children(!right).Parent() = a;
            b.Children(right) = a;
        }
        subtree = b;
        b.Parent() = a.Parent();
        a.Parent() = b;
        a.Balance() = dirSign - b.Balance();
        b.Balance() = -dirSign + b.Balance();
        return result;
    }

    auto RotateSubtreeBig(NodeType* aArg, NodeType* bArg, bool right) -> NodeType*
    {
        using H = TraitsHelper;
        auto a = H(aArg), b = H(bArg), c = H(b.Children(right));
        if (c.Children(right) != a) {
            a.Children(!right) = c.Children(right);
            c.Children(right) = a;
            a.Children(!right).Parent() = a;
        } else {
            a.Children(!right) = c;
        }
        if (c.Children(!right) != b) {
            b.Children(right) = c.Children(!right);
            c.Children(!right) = b;
            b.Children(right).Parent() = b;
        }
        c.Parent() = a.Parent();
        a.Parent() = c;
        b.Parent() = c;
        a.Balance() = ((c.Balance() < 0) ? -(c.Balance()) : 0);
        b.Balance() = ((c.Balance() > 0) ? -(c.Balance()) : 0);
        c.Balance() = 0;
        return c;
    }

    static NodeType* FindNeighbour(NodeType* nodeArg, bool right)
    {
        using H = TraitsHelper;
        auto node = H(nodeArg);
        auto successor = H(node.Children(right));
        while (successor.Children(!right) != node) {
            successor = successor.Children(!right);
        }
        return successor;
    }

    NodeType sentinel;
};

}

#endif // AVL_TREE_H
