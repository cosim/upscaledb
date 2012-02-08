/**
 * Copyright (C) 2005-2011 Christoph Rupp (chris@crupp.de).
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 *
 * See files COPYING.* for License information.
 */

#include "../src/config.h"

#include <stdexcept>
#include <string.h>
#include <vector>
#include <string>
#include <algorithm>
#include <ham/hamsterdb.h>
#include "../src/db.h"
#include "../src/blob.h"
#include "../src/backend.h"
#include "../src/btree.h"
#include "../src/endianswap.h"
#include "../src/cursor.h"
#include "../src/env.h"
#include "../src/btree_cursor.h"
#include "os.hpp"

#include "bfc-testsuite.hpp"
#include "hamster_fixture.hpp"

using namespace bfc;

class ApproxTest : public hamsterDB_fixture
{
	define_super(hamsterDB_fixture);

public:
    ApproxTest(const char *name="ApproxTest")
    :   hamsterDB_fixture(name)
    {
        testrunner::get_instance()->register_fixture(this);
        BFC_REGISTER_TEST(ApproxTest, lessThanTest);
        BFC_REGISTER_TEST(ApproxTest, lessOrEqualTest);
        BFC_REGISTER_TEST(ApproxTest, greaterThanTest);
        BFC_REGISTER_TEST(ApproxTest, greaterOrEqualTest);
    }

protected:
    ham_db_t *m_db;
    ham_txn_t *m_txn;

public:
    virtual void setup() 
	{ 
		__super::setup();

        (void)os::unlink(BFC_OPATH(".test"));

        BFC_ASSERT_EQUAL(0, ham_new(&m_db));
        BFC_ASSERT_EQUAL(0,
                    ham_create(m_db, BFC_OPATH(".test"), 
                            HAM_ENABLE_TRANSACTIONS, 0664));
        BFC_ASSERT_EQUAL(0, 
                    ham_txn_begin(&m_txn, ham_get_env(m_db), 0, 0, 0));
    }

    virtual void teardown() 
	{ 
		__super::teardown();

        BFC_ASSERT_EQUAL(0, ham_txn_abort(m_txn, 0));
        BFC_ASSERT_EQUAL(0, ham_close(m_db, HAM_AUTO_CLEANUP));
        BFC_ASSERT_EQUAL(0, ham_delete(m_db));
    }

    ham_status_t insertBtree(const char *s) {
        ham_key_t k={0};
        k.data=(void *)s;
        k.size=strlen(s)+1;
        ham_record_t r={0};
        r.data=k.data;
        r.size=k.size;

        ham_backend_t *be=((Database *)m_db)->get_backend();
        return (be->_fun_insert(be, &k, &r, 0));
    }

    ham_status_t insertTxn(const char *s) {
        ham_key_t k={0};
        k.data=(void *)s;
        k.size=strlen(s)+1;
        ham_record_t r={0};
        r.data=k.data;
        r.size=k.size;

        return (ham_insert(m_db, m_txn, &k, &r, 0));
    }

    ham_status_t find(ham_u32_t flags, const char *search,
                    const char *expected) {
        ham_key_t k={0};
        k.data=(void *)search;
        k.size=strlen(search)+1;
        ham_record_t r={0};
        ham_status_t st=ham_find(m_db, m_txn, &k, &r, flags);
        if (st)
            return (st);
        return (strcmp(expected, (const char *)r.data));
    }
    
    void lessThanTest(void)
    {
        // btree < nil
        BFC_ASSERT_EQUAL(0, insertBtree("1"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_LT_MATCH, "2", "1"));

        // txn < nil
        BFC_ASSERT_EQUAL(0, insertTxn("2"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_LT_MATCH, "3", "2"));

        // btree < txn
        BFC_ASSERT_EQUAL(0, insertBtree("10"));
        BFC_ASSERT_EQUAL(0, insertTxn("11"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_LT_MATCH, "11", "10"));

        // txn < btree
        BFC_ASSERT_EQUAL(0, insertTxn("20"));
        BFC_ASSERT_EQUAL(0, insertBtree("21"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_LT_MATCH, "21", "20"));

        // btree < btree
        BFC_ASSERT_EQUAL(0, insertBtree("30"));
        BFC_ASSERT_EQUAL(0, insertBtree("31"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_LT_MATCH, "31", "30"));

        // txn < txn
        BFC_ASSERT_EQUAL(0, insertTxn("40"));
        BFC_ASSERT_EQUAL(0, insertTxn("41"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_LT_MATCH, "41", "40"));
    }

    void lessOrEqualTest(void)
    {
        // btree < nil
        BFC_ASSERT_EQUAL(0, insertBtree("1"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_LEQ_MATCH, "2", "1"));

        // btree = nil
        BFC_ASSERT_EQUAL(0, insertBtree("2"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_LEQ_MATCH, "2", "2"));

        // txn < nil
        BFC_ASSERT_EQUAL(0, insertTxn("3"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_LEQ_MATCH, "4", "3"));

        // txn = nil
        BFC_ASSERT_EQUAL(0, insertTxn("4"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_LEQ_MATCH, "5", "4"));

        // btree < txn
        BFC_ASSERT_EQUAL(0, insertBtree("10"));
        BFC_ASSERT_EQUAL(0, insertTxn("11"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_LEQ_MATCH, "11", "10"));

        // txn < btree
        BFC_ASSERT_EQUAL(0, insertTxn("20"));
        BFC_ASSERT_EQUAL(0, insertBtree("21"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_LEQ_MATCH, "21", "20"));

        // btree < btree
        BFC_ASSERT_EQUAL(0, insertBtree("30"));
        BFC_ASSERT_EQUAL(0, insertBtree("31"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_LEQ_MATCH, "31", "30"));

        // txn < txn
        BFC_ASSERT_EQUAL(0, insertTxn("40"));
        BFC_ASSERT_EQUAL(0, insertTxn("41"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_LEQ_MATCH, "41", "30"));

        // txn =
        BFC_ASSERT_EQUAL(0, insertBtree("50"));
        BFC_ASSERT_EQUAL(0, insertTxn("51"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_LEQ_MATCH, "51", "51"));

        // btree =
        BFC_ASSERT_EQUAL(0, insertTxn("60"));
        BFC_ASSERT_EQUAL(0, insertBtree("61"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_LEQ_MATCH, "61", "61"));
    }

    void greaterThanTest(void)
    {
        // btree > nil
        BFC_ASSERT_EQUAL(0, insertBtree("2"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_GT_MATCH, "1", "2"));

        // txn > nil
        BFC_ASSERT_EQUAL(0, insertTxn("2"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_GT_MATCH, "1", "2"));

        // btree > txn
        BFC_ASSERT_EQUAL(0, insertTxn("10"));
        BFC_ASSERT_EQUAL(0, insertBtree("11"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_GT_MATCH, "10", "11"));

        // txn > btree
        BFC_ASSERT_EQUAL(0, insertBtree("20"));
        BFC_ASSERT_EQUAL(0, insertTxn("21"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_GT_MATCH, "20", "21"));

        // btree > btree
        BFC_ASSERT_EQUAL(0, insertBtree("30"));
        BFC_ASSERT_EQUAL(0, insertBtree("31"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_GT_MATCH, "30", "31"));

        // txn > txn
        BFC_ASSERT_EQUAL(0, insertTxn("40"));
        BFC_ASSERT_EQUAL(0, insertTxn("41"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_GT_MATCH, "40", "41"));
    }

    void greaterOrEqualTest(void)
    {
        // btree > nil
        BFC_ASSERT_EQUAL(0, insertBtree("1"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_GEQ_MATCH, "0", "1"));

        // btree = nil
        BFC_ASSERT_EQUAL(0, insertBtree("3"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_GEQ_MATCH, "3", "3"));

        // txn > nil
        BFC_ASSERT_EQUAL(0, insertTxn("5"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_GEQ_MATCH, "4", "5"));

        // txn = nil
        BFC_ASSERT_EQUAL(0, insertTxn("7"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_GEQ_MATCH, "7", "7"));

        // btree > txn
        BFC_ASSERT_EQUAL(0, insertTxn("10"));
        BFC_ASSERT_EQUAL(0, insertBtree("11"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_GEQ_MATCH, "10", "11"));

        // txn > btree
        BFC_ASSERT_EQUAL(0, insertBtree("20"));
        BFC_ASSERT_EQUAL(0, insertTxn("21"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_GEQ_MATCH, "20", "21"));

        // btree > btree
        BFC_ASSERT_EQUAL(0, insertBtree("30"));
        BFC_ASSERT_EQUAL(0, insertBtree("31"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_GEQ_MATCH, "30", "31"));

        // txn > txn
        BFC_ASSERT_EQUAL(0, insertTxn("40"));
        BFC_ASSERT_EQUAL(0, insertTxn("41"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_GEQ_MATCH, "40", "41"));

        // txn =
        BFC_ASSERT_EQUAL(0, insertBtree("50"));
        BFC_ASSERT_EQUAL(0, insertTxn("51"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_GEQ_MATCH, "51", "51"));

        // btree =
        BFC_ASSERT_EQUAL(0, insertTxn("60"));
        BFC_ASSERT_EQUAL(0, insertBtree("61"));
        BFC_ASSERT_EQUAL(0, find(HAM_FIND_GEQ_MATCH, "61", "61"));
    }
};

BFC_REGISTER_FIXTURE(ApproxTest);
