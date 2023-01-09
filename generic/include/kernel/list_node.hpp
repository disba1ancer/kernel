#ifndef LIST_NODE_H
#define LIST_NODE_H

namespace kernel::intrusive {

template <typename Tag = void>
struct ListNode;

template <>
struct ListNode<void> {
    ListNode* prev;
    ListNode* next;
};

template <typename Tag>
struct ListNode : ListNode<> {};

template <typename T>
struct ListNodeTraits;

template <typename T>
struct ListNodeTraits<ListNode<T>> {
    using NodeType = ListNode<T>;
    using SentinelType = ListNode<T>;
    static auto GetNext(NodeType& node) -> NodeType* {
        return node.next;
    }
    static void SetNext(NodeType& node, NodeType* next) {
        node.next = next;
    }
    static auto GetPrev(NodeType& node) -> NodeType* {
        return node.prev;
    }
    static void SetPrev(NodeType& node, NodeType* prev) {
        node.prev = prev;
    }
};

}

#endif // LIST_NODE_H
