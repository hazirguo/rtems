/**
 * @file
 *
 * @brief Simple SMP Scheduler Implementation
 *
 * @ingroup ScoreSchedulerSMP
 */

/*
 * Copyright (c) 2013 embedded brains GmbH.
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rtems.com/license/LICENSE.
 */

#if HAVE_CONFIG_H
  #include "config.h"
#endif

#include <rtems/score/schedulersimplesmp.h>
#include <rtems/score/schedulersimpleimpl.h>
#include <rtems/score/schedulersmpimpl.h>
#include <rtems/score/wkspace.h>

void _Scheduler_simple_smp_Initialize( void )
{
  Scheduler_SMP_Control *self =
    _Workspace_Allocate_or_fatal_error( sizeof( *self ) );

  _Chain_Initialize_empty( &self->ready[ 0 ] );
  _Chain_Initialize_empty( &self->scheduled );

  _Scheduler.information = self;
}

static void _Scheduler_simple_smp_Move_from_scheduled_to_ready(
  Chain_Control *ready_chain,
  Thread_Control *scheduled_to_ready
)
{
  _Chain_Extract_unprotected( &scheduled_to_ready->Object.Node );
  _Scheduler_simple_Insert_priority_lifo( ready_chain, scheduled_to_ready );
}

static void _Scheduler_simple_smp_Move_from_ready_to_scheduled(
  Chain_Control *scheduled_chain,
  Thread_Control *ready_to_scheduled
)
{
  _Chain_Extract_unprotected( &ready_to_scheduled->Object.Node );
  _Scheduler_simple_Insert_priority_fifo( scheduled_chain, ready_to_scheduled );
}

static void _Scheduler_simple_smp_Insert(
  Chain_Control *chain,
  Thread_Control *thread,
  Chain_Node_order order
)
{
  _Chain_Insert_ordered_unprotected( chain, &thread->Object.Node, order );
}

static void _Scheduler_simple_smp_Enqueue_ordered(
  Thread_Control *thread,
  Chain_Node_order order
)
{
  Scheduler_SMP_Control *self = _Scheduler_SMP_Instance();

  /*
   * The scheduled chain has exactly processor count nodes after
   * initialization, thus the lowest priority scheduled thread exists.
   */
  Thread_Control *lowest_scheduled =
    (Thread_Control *) _Chain_Last( &self->scheduled );

  if ( ( *order )( &thread->Object.Node, &lowest_scheduled->Object.Node ) ) {
    _Scheduler_SMP_Allocate_processor( thread, lowest_scheduled );

    _Scheduler_simple_smp_Insert( &self->scheduled, thread, order );

    _Scheduler_simple_smp_Move_from_scheduled_to_ready(
      &self->ready[ 0 ],
      lowest_scheduled
    );
  } else {
    _Scheduler_simple_smp_Insert( &self->ready[ 0 ], thread, order );
  }
}

void _Scheduler_simple_smp_Enqueue_priority_lifo( Thread_Control *thread )
{
  _Scheduler_simple_smp_Enqueue_ordered(
    thread,
    _Scheduler_simple_Insert_priority_lifo_order
  );
}

void _Scheduler_simple_smp_Enqueue_priority_fifo( Thread_Control *thread )
{
  _Scheduler_simple_smp_Enqueue_ordered(
    thread,
    _Scheduler_simple_Insert_priority_fifo_order
  );
}

void _Scheduler_simple_smp_Extract( Thread_Control *thread )
{
  Scheduler_SMP_Control *self = _Scheduler_SMP_Instance();

  _Chain_Extract_unprotected( &thread->Object.Node );

  if ( thread->is_scheduled ) {
    Thread_Control *highest_ready =
      (Thread_Control *) _Chain_First( &self->ready[ 0 ] );

    _Scheduler_SMP_Allocate_processor( highest_ready, thread );

    _Scheduler_simple_smp_Move_from_ready_to_scheduled(
      &self->scheduled,
      highest_ready
    );
  }
}

void _Scheduler_simple_smp_Yield( Thread_Control *thread )
{
  ISR_Level level;

  _ISR_Disable( level );

  _Scheduler_simple_smp_Extract( thread );
  _Scheduler_simple_smp_Enqueue_priority_fifo( thread );

  _ISR_Enable( level );
}

void _Scheduler_simple_smp_Schedule( Thread_Control *thread )
{
  ( void ) thread;
}
