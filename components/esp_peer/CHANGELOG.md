# Changelog

## v1.2.2

### Bug Fixes

- Fixed build error for component name miss matched

## v1.2.1

### Features

- Make `esp_peer` as separate module for ESP Component Registry
- Allow RTP rolling buffer allocated on RAM

### Bug Fixes

- Fixed handshake may error if agent receive handshake message during connectivity check

## v1.2.0

### Features

- Added reliable and un-ordered data channel support
- Added FORWARD-TSN support for un-ordered data channel

### Bug Fixes

- Fixed keep alive check not take effect if agent only have local candidates
- Fixed agent mode not set if remote candidate already get

## v1.1.0

### Features

- Export buffer configuration for RTP and data channel
- Added support for multiple data channel
- Added notify for data channel open, close event

### Bug Fixes

- Fixed keep alive check not take effect if peer closed unexpectedly


## v1.0.0

- Initial version of `peer_default`
