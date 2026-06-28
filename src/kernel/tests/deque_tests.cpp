#include <tests/ktest.hpp>
#include <std/single_deque.hpp>

namespace Tests {
    namespace {
        struct Item {
            int id;
            kstd::SingleNode hook;
        };

        using ItemDeque = kstd::SingleDeque<Item, &Item::hook>;

        // Walk the list and confirm the live nodes are *exactly* the pointers we
        // pushed, in order. Intrusive lists never copy, so equality is pointer
        // identity — this is what "pointer stability" means here.
        bool order_matches(ItemDeque& dq, Item* const* expected, std::size_t n) {
            std::size_t i = 0;
            for (auto& item : dq) {
                if (i >= n || &item != expected[i]) {
                    return false;
                }
                i++;
            }
            return i == n;
        }
    }

    void deque_tests() {
        Util::klog("--- Deque Tests ---\n");
        KTest::reset();

        Item a { 1, {} };
        Item b { 2, {} };
        Item c { 3, {} };
        Item d { 4, {} };

        // ── size() tracks pushes and pops ────────────────────────────────────
        {
            ItemDeque dq {};
            KTEST_ASSERT(dq.size() == 0, "fresh deque has size 0");
            KTEST_ASSERT(dq.empty(),     "fresh deque is empty");

            dq.push_tail(&a);
            dq.push_tail(&b);
            dq.push_tail(&c);
            KTEST_ASSERT(dq.size() == 3, "size is 3 after three push_tail");
            KTEST_ASSERT(!dq.empty(),    "deque is not empty after pushes");

            dq.push_head(&d);
            KTEST_ASSERT(dq.size() == 4, "push_head also grows size");

            dq.pop_head();
            KTEST_ASSERT(dq.size() == 3, "pop_head shrinks size");

            dq.pop_tail();
            KTEST_ASSERT(dq.size() == 2, "pop_tail shrinks size");

            dq.clear_all();
            KTEST_ASSERT(dq.size() == 0, "clear_all resets size to 0");
            KTEST_ASSERT(dq.empty(),     "clear_all leaves deque empty");
        }

        // ── pop returns the same objects we pushed (identity, not copies) ─────
        {
            ItemDeque dq {};
            dq.push_tail(&a);
            dq.push_tail(&b);
            dq.push_tail(&c);

            Item* h = dq.pop_head();
            KTEST_ASSERT(h == &a,    "pop_head returns the exact head pointer");
            KTEST_ASSERT(h->id == 1, "popped object is unmodified (id intact)");

            Item* t = dq.pop_tail();
            KTEST_ASSERT(t == &c,    "pop_tail returns the exact tail pointer");
            KTEST_ASSERT(t->id == 3, "popped tail object is unmodified");

            KTEST_ASSERT(dq.size() == 1,        "one element remains after two pops");
            KTEST_ASSERT(&*dq.begin() == &b,    "remaining node is the same &b we pushed");

            dq.clear_all();
        }

        // ── remove() from the middle keeps the other pointers stable ─────────
        {
            ItemDeque dq {};
            dq.push_tail(&a);
            dq.push_tail(&b);
            dq.push_tail(&c);
            dq.push_tail(&d);

            dq.remove(&b);
            auto _size = dq.size();
            KTEST_ASSERT(_size == 3, "remove() of a middle node shrinks size by one");

            Item* expected[] = { &a, &c, &d };
            KTEST_ASSERT(order_matches(dq, expected, 3),
                         "survivors keep their identity and order after middle remove");
            KTEST_ASSERT(a.id == 1 && c.id == 3 && d.id == 4,
                         "untouched objects are not mutated by remove()");

            dq.clear_all();
        }

        // ── remove() of the head and tail ends ───────────────────────────────
        {
            ItemDeque dq {};
            dq.push_tail(&a);
            dq.push_tail(&b);
            dq.push_tail(&c);

            dq.remove(&a); // head
            Item* after_head[] = { &b, &c };
            KTEST_ASSERT(dq.size() == 2 && order_matches(dq, after_head, 2),
                         "removing the head leaves the rest stable");

            dq.remove(&c); // tail
            Item* after_tail[] = { &b };
            KTEST_ASSERT(dq.size() == 1 && order_matches(dq, after_tail, 1),
                         "removing the tail leaves the rest stable");

            dq.remove(&b); // last one
            KTEST_ASSERT(dq.size() == 0 && dq.empty(),
                         "removing the final node empties the deque");

            dq.clear_all();
        }

        {
            ItemDeque dq {};
            dq.push_head(&a);
            dq.push_head(&b);
            dq.push_head(&c);

            dq.remove(&a); // head
            Item* after_head[] = { &c, &b};
            KTEST_ASSERT(dq.size() == 2 && order_matches(dq, after_head, 2),
                         "push head test");

            dq.remove(&c); // tail
            Item* after_tail[] = { &b };
            KTEST_ASSERT(dq.size() == 1 && order_matches(dq, after_tail, 1),
                         "push head test");

            dq.remove(&b); // last one
            KTEST_ASSERT(dq.size() == 0 && dq.empty(),
                         "push head test");

            dq.clear_all();
        }

        // tests to make sure that inserting and deleting middle doesn't case pointer issues. 
        {
            ItemDeque dq {};

            dq.push_tail(&a);
            dq.push_tail(&b);
            dq.push_tail(&c);

            KTEST_ASSERT(dq.size() == 3, "size");

            dq.remove(&b);
            Item* after_head[] = { &a, &c }; 

            KTEST_ASSERT(dq.size() == 2 && order_matches(dq, after_head, 2),
                         "removing the middle leaves the rest stable");
            
            dq.push_tail(&d);
            Item* item[] = { &a, &c, &d }; 
            KTEST_ASSERT(dq.size() == 3 && order_matches(dq, item, 3),
                         "adding the middle leaves the rest stable");
            dq.clear_all();
        }

        KTest::summary("Deque");
    }
}
