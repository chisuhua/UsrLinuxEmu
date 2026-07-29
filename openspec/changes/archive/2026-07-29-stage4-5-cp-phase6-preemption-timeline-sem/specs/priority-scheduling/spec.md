## ADDED Requirements

### Requirement: Multi-level priority queues

ChannelManager SHALL maintain three priority levels (`GPU_CHAN_PRI_HIGH`, `GPU_CHAN_PRI_NORMAL`, `GPU_CHAN_PRI_LOW`) with separate FIFO queues per level. Channel priority SHALL be assigned at creation time and SHALL NOT change during channel lifetime.

#### Scenario: HIGH priority channel scheduled before LOW priority channel

- **GIVEN** channel A (priority=HIGH) and channel B (priority=LOW) are both in the ready queue
- **WHEN** scheduler selects the next channel via `selectNextChannel()`
- **THEN** channel A SHALL be selected before channel B
- **THEN** this SHALL hold regardless of insertion order

#### Scenario: Starvation protection forces LOW priority dequeue

- **GIVEN** there are ready HIGH priority channels and `kStarvationThreshold=10` scheduling cycles have elapsed since last LOW priority channel was selected
- **WHEN** scheduler calls `selectNextChannel()`
- **THEN** exactly 1 LOW priority entry SHALL be forcibly dequeued in the 10th cycle
- **THEN** the starvation counter SHALL reset after forced dequeue

### Requirement: Starvation counter

The scheduler SHALL maintain a starvation counter that increments each scheduling cycle when only non-LOW priority channels are selected. When the counter reaches `kStarvationThreshold`, exactly 1 LOW priority entry SHALL be serviced.

#### Scenario: Counter increment without LOW priority service

- **GIVEN** only HIGH priority channels are ready
- **WHEN** scheduler selects a HIGH priority channel for 9 consecutive cycles
- **THEN** starvation counter SHALL be 9 after the 9th cycle

#### Scenario: Counter resets after forced dequeue

- **GIVEN** starvation counter is at `kStarvationThreshold`
- **WHEN** scheduler forcibly dequeues 1 LOW priority entry
- **THEN** starvation counter SHALL reset to 0
