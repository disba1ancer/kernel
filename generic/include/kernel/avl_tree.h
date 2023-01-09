#ifndef AVLTREE_H
#define AVLTREE_H

#include <concepts>
#include <cstdlib>
#include "node.h"
#include "util.hpp"

namespace kernel::intrusive {

namespace detail {

template <typename Comp, typename Key>
concept Comparator = requires (const Key t1, const Key t2, Comp c)
{
    { c(t1, t2) } -> std::same_as<bool>;
};

}

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
struct AVLTreeNode : public AVLTreeNode<> {};

template <
    typename T,
    detail::Comparator<T> Comp,
    typename CastPolicyGen = BaseClassCastPolicy<void>
>
class AVLTree : detail::ContainerNodeRequirments<AVLTreeNode, T, CastPolicyGen> {
public:
    using Node = AVLTreeNode<>;
    using CastPolicy = typename CastPolicyGen::template Policy<T, AVLTreeNode>;
    AVLTree()
    {
        sentinel.children[0] = &sentinel;
        sentinel.children[1] = &sentinel;
    }

    class Iterator : public detail::BasicIterator<Iterator, T> {
        friend class AVLTree;
        Iterator(Node* current) noexcept :
            current(current)
        {}

        auto next(bool right) noexcept -> Iterator&
        {
            auto possibleNext = current->children[right];
            if (possibleNext->parent != current) {
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
            return CastPolicy::GetPtrFromHeader(current);
        }

        bool operator!=(const Iterator& oth) const noexcept
        {
            return current != oth.current;
        }
    private:
        Node* current;
    };

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
        using cp = CastPolicy;
        Node* erasedNode = cp::GetHeaderFromPtr((it++).operator->());
        bool a = erasedNode->children[0]->parent == erasedNode;
        bool b = erasedNode->children[1]->parent == erasedNode;
        if (!(a && b)) {
            auto children = erasedNode->children;
            bool c = erasedNode->parent->children[0] == erasedNode;
            if (a == b) {
                bool c = erasedNode->children[0] == erasedNode->parent;
                erasedNode->parent->children[c] = erasedNode->children[c];
            } else {
                children[b]->parent = erasedNode->parent;
                children[b]->parent->children[a] = children[b];
                children[b]->children[a] = children[a];
            }
            RebalanceTreeE(erasedNode->parent, c);
            return it;
        }
        Node neighbours;
        neighbours.children[0] = FindNeighbour(erasedNode, false);
        neighbours.children[1] = cp::GetHeaderFromPtr(it.operator->());
        bool c = erasedNode->balance <= 0;
        Node* lowerNode = neighbours.children[c];
        Node* parent = lowerNode->parent;
        neighbours.children[!c]->children[c] = lowerNode;
        if (lowerNode->children[c] != parent) {
            lowerNode->children[c]->parent = parent;
            parent->children[!c] = lowerNode->children[c];
        }
        bool d = erasedNode->parent->children[0] != erasedNode;
        lowerNode->parent = erasedNode->parent;
        lowerNode->parent->children[d] = lowerNode;

        if (erasedNode->children[0] != lowerNode) {
            lowerNode->children[0] = erasedNode->children[0];
            lowerNode->children[0]->parent = lowerNode;
        }

        if (erasedNode->children[1] != lowerNode) {
            lowerNode->children[1] = erasedNode->children[1];
            lowerNode->children[1]->parent = lowerNode;
        }

        lowerNode->balance = erasedNode->balance;

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
        using cp = CastPolicy;
        Erase(Iterator(cp::GetHeaderFromPtr(AddressOf(elem))));
        return 1;
    }

    template <typename KeyType>
    std::size_t Erase(const KeyType& elem)
    {
        return EraseInternal(LowerBound(elem), UpperBound(elem));
    }

    Iterator Begin()
    {
        return sentinel.children[1];
    }

    friend Iterator begin(AVLTree& tree)
    {
        return tree.Begin();
    }

    Iterator End()
    {
        return &sentinel;
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
        return cp::GetHeaderFromPtr(AddressOf(elem));
    }

    auto Find(const T& key) -> Iterator {
        return Find<T>(key);
    }

    template <std::same_as<T> KeyType = T>
    requires(requires (const T& t, const KeyType& k, Comp comp) {
        comp(k, t);
        comp(t, k);
    })
    auto Find(const KeyType& key) -> Iterator
    {
        using cp = CastPolicy;
        Node* subtree = sentinel.children[1];
        if (subtree == &sentinel) {
            return &sentinel;
        }
        Node* range[2] = { &sentinel, &sentinel };
        while (true) {
            Comp comp;
            bool a = comp(key, *cp::GetPtrFromHeader(subtree));
            bool b = comp(*cp::GetPtrFromHeader(subtree), key);
            if (a == b) return subtree;
            if (subtree->children[b] == range[b]) {
                return &sentinel;
            }
            range[a] = subtree;
            subtree = subtree->children[b];
        }
        return &sentinel;
    }

    template <typename KeyType>
    auto UpperBound(const KeyType& key) -> Iterator
    {
        using cp = CastPolicy;
        Node* subtree = sentinel.children[1];
        if (subtree == &sentinel) {
            return &sentinel;
        }
        Node* range[2] = { &sentinel, &sentinel };
        while (true) {
            Comp comp;
            bool a = comp(key, *cp::GetPtrFromHeader(subtree));
            if (subtree->children[!a] == range[!a]) {
                return a ? subtree : subtree->children[1];
            }
            range[a] = subtree;
            subtree = subtree->children[!a];
        }
        return &sentinel;
    }

    template <typename KeyType>
    auto LowerBound(const KeyType& key) -> Iterator
    {
        using cp = CastPolicy;
        Node* subtree = sentinel.children[1];
        if (subtree == &sentinel) {
            return &sentinel;
        }
        Node* range[2] = { &sentinel, &sentinel };
        while (true) {
            Comp comp;
            bool a = comp(*cp::GetPtrFromHeader(subtree), key);
            if (subtree->children[a] == range[a]) {
                return a ? subtree->children[1] : subtree;
            }
            range[!a] = subtree;
            subtree = subtree->children[a];
        }
        return &sentinel;
    }

    bool Empty()
    {
        return !(Begin() != End());
    }
private:
    Iterator InsertUnrestricted(Iterator hint, T& elem)
    {
        using cp = CastPolicy;
        Node* parentNode = cp::GetHeaderFromPtr(hint.operator->());
        bool rightInsert = parentNode->children[0]->parent == parentNode;
        if (rightInsert) {
            parentNode = FindNeighbour(parentNode, false);
        }
        Node* node = cp::GetHeaderFromPtr(AddressOf(elem));
        node->balance = 0;
        node->parent = parentNode;
        node->children[!rightInsert] = parentNode;
        node->children[rightInsert] = parentNode->children[rightInsert];
        if (node->children[0] == &sentinel) {
            sentinel.children[1] = node;
        }
        parentNode->children[rightInsert] = node;
        RebalanceTreeI(parentNode, rightInsert);
        return node;
    }

    void RebalanceTreeI(Node* from, bool balanceSign)
    {
        while (from != &sentinel) {
            int balanceDiff = 1 - 2 * balanceSign;
            from->balance += balanceDiff;
            auto nextNode = from->parent;
            bool chInd = nextNode->children[0] != from;
            if (
                from->balance == 0 ||
                (std::abs(from->balance) == 2 &&
                RotateSubtree(nextNode->children[chInd], from->balance > 0))
            ) {
                return;
            }
            balanceSign = chInd;
            from = nextNode;
        }
    }

    std::size_t EraseInternal(Iterator b, Iterator e)
    {
        std::size_t count = 0;
        for (auto i = b; i != e; i = Erase(i)) { ++count; }
        return count;
    }

    void RebalanceTreeE(Node* from, bool balanceSign)
    {
        while (from != &sentinel) {
            int balanceDiff = 1 - 2 * balanceSign;
            from->balance += balanceDiff;
            auto nextNode = from->parent;
            bool chInd = nextNode->children[0] != from;
            if (
                from->balance != 0 &&
                std::abs(from->balance) == 2 &&
                !RotateSubtree(nextNode->children[chInd], from->balance > 0)
            ) {
                return;
            }
            balanceSign = !chInd;
            from = nextNode;
        }
    }

    bool RotateSubtree(Node*& subtree, bool right)
    {
        Node *a = subtree, *b = subtree->children[!right];
        int dirSign = right * 2 - 1;
        if (b->balance * dirSign < 0) {
            subtree = RotateSubtreeBig(a, b, right);
            return true;
        }
        bool result = (b->balance != 0);
        if (b->children[right] != a) {
            a->children[!right] = b->children[right];
            a->children[!right]->parent = a;
            b->children[right] = a;
        }
        subtree = b;
        b->parent = a->parent;
        a->parent = b;
        a->balance = dirSign - b->balance;
        b->balance = -dirSign + b->balance;
        return result;
    }

    auto RotateSubtreeBig(Node* a, Node* b, bool right) -> Node*
    {
        Node* c = b->children[right];
        if (c->children[right] != a) {
            a->children[!right] = c->children[right];
            c->children[right] = a;
            a->children[!right]->parent = a;
        } else {
            a->children[!right] = c;
        }
        if (c->children[!right] != b) {
            b->children[right] = c->children[!right];
            c->children[!right] = b;
            b->children[right]->parent = b;
        }
        c->parent = a->parent;
        a->parent = c;
        b->parent = c;
        a->balance = ((c->balance < 0) ? -(c->balance) : 0);
        b->balance = ((c->balance > 0) ? -(c->balance) : 0);
        c->balance = 0;
        return c;
    }

    static Node* FindNeighbour(Node* node, bool right)
    {
        auto successor = node->children[right];
        while (successor->children[!right] != node) {
            successor = successor->children[!right];
        }
        return successor;
    }

    Node sentinel;
};

}

#endif // AVLTREE_H
