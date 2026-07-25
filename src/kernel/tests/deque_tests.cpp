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

    TEST_CASE("size() tracks pushes and pops", "[deque]") {
        Item a { 1, {} };
        Item b { 2, {} };
        Item c { 3, {} };
        Item d { 4, {} };

        ItemDeque dq {};
        CHECK(dq.size() == 0);
        CHECK(dq.empty());

        dq.push_tail(&a);
        dq.push_tail(&b);
        dq.push_tail(&c);
        CHECK(dq.size() == 3);
        CHECK_FALSE(dq.empty());

        dq.push_head(&d);
        CHECK(dq.size() == 4);

        dq.pop_head();
        CHECK(dq.size() == 3);

        dq.pop_tail();
        CHECK(dq.size() == 2);

        dq.clear_all();
        CHECK(dq.size() == 0);
        CHECK(dq.empty());
    }

    TEST_CASE("pop returns the same objects we pushed, not copies", "[deque]") {
        Item a { 1, {} };
        Item b { 2, {} };
        Item c { 3, {} };

        ItemDeque dq {};
        dq.push_tail(&a);
        dq.push_tail(&b);
        dq.push_tail(&c);

        Item* h = dq.pop_head();
        CHECK(h == &a);
        CHECK(h->id == 1);

        Item* t = dq.pop_tail();
        CHECK(t == &c);
        CHECK(t->id == 3);

        CHECK(dq.size() == 1);
        CHECK(&*dq.begin() == &b);

        dq.clear_all();
    }

    TEST_CASE("remove() from the middle keeps the other pointers stable", "[deque]") {
        Item a { 1, {} };
        Item b { 2, {} };
        Item c { 3, {} };
        Item d { 4, {} };

        ItemDeque dq {};
        dq.push_tail(&a);
        dq.push_tail(&b);
        dq.push_tail(&c);
        dq.push_tail(&d);

        dq.remove(&b);
        CHECK(dq.size() == 3);

        Item* expected[] = { &a, &c, &d };
        CHECK(order_matches(dq, expected, 3));
        CHECK(a.id == 1 && c.id == 3 && d.id == 4);

        dq.clear_all();
    }

    TEST_CASE("remove() of the head and tail ends", "[deque]") {
        Item a { 1, {} };
        Item b { 2, {} };
        Item c { 3, {} };

        ItemDeque dq {};
        dq.push_tail(&a);
        dq.push_tail(&b);
        dq.push_tail(&c);

        dq.remove(&a); // head
        Item* after_head[] = { &b, &c };
        CHECK(dq.size() == 2 && order_matches(dq, after_head, 2));

        dq.remove(&c); // tail
        Item* after_tail[] = { &b };
        CHECK(dq.size() == 1 && order_matches(dq, after_tail, 1));

        dq.remove(&b); // last one
        CHECK(dq.size() == 0 && dq.empty());

        dq.clear_all();
    }

    TEST_CASE("remove() walks a push_head-built deque correctly", "[deque]") {
        Item a { 1, {} };
        Item b { 2, {} };
        Item c { 3, {} };

        ItemDeque dq {};
        dq.push_head(&a);
        dq.push_head(&b);
        dq.push_head(&c);

        dq.remove(&a); // head
        Item* after_head[] = { &c, &b };
        CHECK(dq.size() == 2 && order_matches(dq, after_head, 2));

        dq.remove(&c); // tail
        Item* after_tail[] = { &b };
        CHECK(dq.size() == 1 && order_matches(dq, after_tail, 1));

        dq.remove(&b); // last one
        CHECK(dq.size() == 0 && dq.empty());

        dq.clear_all();
    }

    // makes sure that inserting and deleting middle doesn't cause pointer issues
    TEST_CASE("pushing again after a middle remove keeps the rest stable", "[deque]") {
        Item a { 1, {} };
        Item b { 2, {} };
        Item c { 3, {} };
        Item d { 4, {} };

        ItemDeque dq {};
        dq.push_tail(&a);
        dq.push_tail(&b);
        dq.push_tail(&c);

        REQUIRE(dq.size() == 3);

        dq.remove(&b);
        Item* after_remove[] = { &a, &c };
        CHECK(dq.size() == 2 && order_matches(dq, after_remove, 2));

        dq.push_tail(&d);
        Item* after_push[] = { &a, &c, &d };
        CHECK(dq.size() == 3 && order_matches(dq, after_push, 3));

        dq.clear_all();
    }
}
