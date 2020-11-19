/*****************************************************************************

Copyright (c) 1997, 2019, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/read0types.h
 Cursor read

 Created 2/16/1997 Heikki Tuuri
 *******************************************************/

#ifndef read0types_h
#define read0types_h

#include <algorithm>
#include "dict0mem.h"

#include "trx0types.h"

/** View is not visible to purge thread. */
constexpr int32_t READ_VIEW_STATE_CLOSED = 0;

/** View is being opened, purge thread must wait for state change. */
constexpr int32_t READ_VIEW_STATE_SNAPSHOT = 1;

/** View is visible to purge thread. */
constexpr int32_t READ_VIEW_STATE_OPEN = 2;

/** Read view lists the trx ids of those transactions for which a consistent
read should not see the modifications to the database. */

class ReadView {
 public:
  ReadView();
  ~ReadView();
  /** Check whether transaction id is valid.
  @param[in]	id		transaction id to check
  @param[in]	name		table name */
  static void check_trx_id_sanity(trx_id_t id, const table_name_t &name);

  /** Check whether the changes by id are visible.
  @param[in]	id	transaction id to check against the view
  @param[in]	name	table name
  @return whether the view sees the modifications of id. */
  bool changes_visible(trx_id_t id, const table_name_t &name) const
      MY_ATTRIBUTE((warn_unused_result)) {
    ut_ad(id > 0);

    if (id < m_up_limit_id || id == m_creator_trx_id) {
      return (true);
    }

    check_trx_id_sanity(id, name);

    if (id >= m_low_limit_id) {
      return (false);

    } else if (m_ids.empty()) {
      return (true);
    }

    return (!std::binary_search(m_ids.begin(), m_ids.end(), id));
  }

  /**
  @param id		transaction to check
  @return true if view sees transaction id */
  bool sees(trx_id_t id) const { return (id < m_up_limit_id); }

  /** Creates a snapshot where exactly the transaction serialized before this
  point in time are seen in the view.

  @param[in, out] trx transaction */
  void snapshot(trx_t *trx);

  /** Open a read view where exactly the transaction serialized before this
  point in time are seen in the view.

  View become visible to purge thread.

  @param[in,out] trx transaction */
  void open(trx_t *trx);

  /**
  Mark the view as closed */
  void close() {
    int32_t state = m_state.load(std::memory_order_relaxed);
    ut_ad(state == READ_VIEW_STATE_CLOSED || state == READ_VIEW_STATE_OPEN);
    if (state == READ_VIEW_STATE_OPEN) {
      m_state.store(READ_VIEW_STATE_CLOSED, std::memory_order_relaxed);
    }
  }

  /** m_state getter fir trx_sys::clone_oldest_view() & trx_sys::size(). */
  int32_t get_state() const { return m_state.load(std::memory_order_acquire); }

  /** Returns ture if view is open.

  Only used by view owner thread, thus we can omit atomic operations. */
  bool is_open() const {
    int32_t state = m_state.load(std::memory_order_relaxed);
    ut_ad(state == READ_VIEW_STATE_OPEN || state == READ_VIEW_STATE_CLOSED);
    return (state == READ_VIEW_STATE_OPEN);
  }

  /** Set the creator transaction id.

  This should be set only for views created by RW transactions. */
  void set_creator_trx_id(trx_id_t id) {
    ut_ad(id > 0);
    ut_ad(m_creator_trx_id == 0);
    m_creator_trx_id = id;
  }

  /**
  Write the limits to the file.
  @param file		file to write to */
  void print_limits(FILE *file) const {
    fprintf(file,
            "Trx read view will not see trx with"
            " id >= " TRX_ID_FMT ", sees < " TRX_ID_FMT "\n",
            m_low_limit_id, m_up_limit_id);
  }

  /** Check and reduce low limit number for read view. Used to
  block purge till GTID is persisted on disk table.
  @param[in]	trx_no	transaction number to check with */
  void reduce_low_limit(trx_id_t trx_no) {
    if (trx_no < m_low_limit_no) {
      /* Save low limit number set for Read View for MVCC. */
      ut_d(m_view_low_limit_no = m_low_limit_no);
      m_low_limit_no = trx_no;
    }
  }

  /**
  @return the low limit no */
  trx_id_t low_limit_no() const { return (m_low_limit_no); }

  /**
  @return the low limit id */
  trx_id_t low_limit_id() const { return (m_low_limit_id); }

  /**
  @return true if there are no transaction ids in the snapshot */
  bool empty() const { return (m_ids.empty()); }

#ifdef UNIV_DEBUG
  /**
  @return the view low limit number */
  trx_id_t view_low_limit_no() const { return (m_view_low_limit_no); }

  /**
  @param rhs		view to compare with
  @return truen if this view is less than or equal rhs */
  bool le(const ReadView *rhs) const {
    return (m_low_limit_no <= rhs->m_low_limit_no);
  }
#endif /* UNIV_DEBUG */

  void copy(const ReadView &other);

 private:
  // Disable copying
  ReadView(const ReadView &);
  ReadView &operator=(const ReadView &);

 private:
  /** The read should not see any transaction with trx id >= this
  value. In other words, this is the "high water mark". */
  trx_id_t m_low_limit_id;

  /** The read should see all trx ids which are strictly
  smaller (<) than this value.  In other words, this is the
  low water mark". */
  trx_id_t m_up_limit_id;

  /** trx id of creating transaction, set to TRX_ID_MAX for free
  views. */
  trx_id_t m_creator_trx_id;

  /** Set of RW transactions that was active when this snapshot
  was taken */
  trx_ids_t m_ids;

  /** The view does not need to see the undo logs for transactions
  whose transaction number is strictly smaller (<) than this value:
  they can be removed in purge if not needed by other views */
  trx_id_t m_low_limit_no;

#ifdef UNIV_DEBUG
  /** The low limit number up to which read views don't need to access
  undo log records for MVCC. This could be higher than m_low_limit_no
  if purge is blocked for GTID persistence. Currently used for debug
  variable INNODB_PURGE_VIEW_TRX_ID_AGE. */
  trx_id_t m_view_low_limit_no;
#endif /* UNIV_DEBUG */

  /** View state.

  It is not defined as enum as it has to be updated using atomic operations.
  Possible values are READ_VIEW_STATE_CLOSED, READ_VIEW_STATE_SNAPSHOT and
  READ_VIEW_STATE_OPEN.

  Possible state transfers...

  Start view open:
  READ_VIEW_STATE_CLOSED -> READ_VIEW_STATE_SNAPSHOT

  Complete view open:
  READ_VIEW_STATE_SNAPSHOT -> READ_VIEW_STATE_OPEN

  Close view:
  READ_VIEW_STATE_OPEN -> READ_VIEW_STATE_CLOSED */
  std::atomic<int32_t> m_state;
};

#endif
