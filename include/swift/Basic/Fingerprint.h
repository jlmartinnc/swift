//===--- Fingerprint.h - A stable identity for compiler data ----*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2020 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_BASIC_FINGERPRINT_H
#define SWIFT_BASIC_FINGERPRINT_H

#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MD5.h"

#include <string>

namespace llvm {
namespace yaml {
class IO;
}
}; // namespace llvm

namespace swift {

/// A \c Fingerprint represents a stable summary of a given piece of data
/// in the compiler.
///
/// A \c Fingerprint value is subject to the following invariants:
/// 1) For two values \c x and \c y of type T, if \c T::operator==(x, y) is
///    \c true, then the Fingerprint of \c x and the Fingerprint of \c y must be
///    equal.
/// 2) For two values \c x and \c y of type T, the chance of a collision in
///    fingerprints is a rare occurrence - especially if \c y is a minor
///    perturbation of \c x.
/// 3) The \c Fingerprint value is required to be stable *across compilation
///    sessions*.
///
/// Property 3) is the most onerous. It implies that data like addresses, file
/// paths, and other ephemeral compiler state *may not* be used as inputs to the
/// fingerprint generation function.
///
/// \c Fingerprint values are currently used in two places by the compiler's
/// dependency tracking subsystem. They are used at the level of files to detect
/// when tokens (outside of the body of a function or an iterable decl context)
/// have been perturbed. Additionally, they are used at the level of individual
/// iterable decl contexts to detect when the tokens in their bodies have
/// changed. This makes them a coarse - yet safe - overapproximation for when a
/// decl has changed semantically.
///
/// \c Fingerprints are currently implemented as a thin wrapper around an MD5
/// hash. MD5 is known to be neither the fastest nor the most
/// cryptographically capable algorithm, but it does afford us the avalanche
/// effect we desire. We should revisit the modeling decision here.
class Fingerprint final {
public:
  /// The size (in bytes) of the raw value of all fingerprints.
  ///
  /// This constant's value is justified by a static assertion in the
  /// corresponding cpp file.
  constexpr static size_t DIGEST_LENGTH = 32;

private:
  std::string Core;

public:
  /// Creates a fingerprint value from the given input string that is known to
  /// be a 32-byte hash value.
  ///
  /// In +asserts builds, strings that violate this invariant will crash. If a
  /// fingerprint value is needed to represent an "invalid" state, use a
  /// vocabulary type like \c Optional<Fingerprint> instead.
  explicit Fingerprint(std::string value) : Core(std::move(value)) {
    assert(Core.size() == Fingerprint::DIGEST_LENGTH &&
           "Only supports 32-byte hash values!");
  }

  /// Creates a fingerprint value from the given input string literal.
  template <std::size_t N>
  explicit Fingerprint(const char (&literal)[N])
    : Core{literal, N-1} {
      static_assert(N == Fingerprint::DIGEST_LENGTH + 1,
                    "String literal must be 32 bytes in length!");
    }

  /// Creates a fingerprint value by consuming the given \c MD5Result from LLVM.
  explicit Fingerprint(llvm::MD5::MD5Result &&MD5Value)
      : Core{MD5Value.digest().str()} {}

public:
  /// Retrieve the raw underlying bytes of this fingerprint.
  llvm::StringRef getRawValue() const { return Core; }

public:
  friend bool operator==(const Fingerprint &lhs, const Fingerprint &rhs) {
    return lhs.Core == rhs.Core;
  }

  friend bool operator!=(const Fingerprint &lhs, const Fingerprint &rhs) {
    return lhs.Core != rhs.Core;
  }

  friend llvm::hash_code hash_value(const Fingerprint &fp) {
    return llvm::hash_value(fp.Core);
  }

public:
  /// The fingerprint value consisting of 32 bytes of zeroes.
  ///
  /// This fingerprint is a perfectly fine value for an MD5 hash, but it is
  /// completely arbitrary.
  static Fingerprint ZERO() {
    return Fingerprint("00000000000000000000000000000000");
  }

private:
  /// llvm::yaml would like us to be default constructible, but \c Fingerprint
  /// would prefer to enforce its internal invariants.
  ///
  /// Very well, LLVM. A default value you shall have.
  friend class llvm::yaml::IO;
  Fingerprint() : Core{DIGEST_LENGTH, '0'} {}
};

void simple_display(llvm::raw_ostream &out, const Fingerprint &fp);
}; // namespace swift

namespace llvm {
class raw_ostream;
raw_ostream &operator<<(raw_ostream &OS, const swift::Fingerprint &fp);
}; // namespace llvm

#endif // SWIFT_BASIC_FINGERPRINT_H
