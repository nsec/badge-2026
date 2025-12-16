# Conference Library

Conference-specific features for NorthSec Badge 2026.

This library contains modules that are only included in the **factory** firmware partition.

## Purpose

The factory partition serves as the conference badge firmware with features like:
- Badge-to-badge communication
- Schedule/event information
- Social/networking features
- Conference-specific utilities

## Usage

The conference modules are automatically initialized when running from the factory partition.

See `registry.cpp` for the initialization entry point.
