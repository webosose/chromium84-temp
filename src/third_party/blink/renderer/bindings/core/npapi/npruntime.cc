/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007-2009 Google, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "base/logging.h"
#include "third_party/blink/renderer/bindings/core/npapi/np_v8_object.h"
#include "third_party/blink/renderer/bindings/core/npapi/v8_np_object.h"
#include "third_party/blink/renderer/bindings/core/npapi/npruntime_impl.h"
#include "third_party/blink/renderer/bindings/core/npapi/npruntime_priv.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include <stdlib.h>
#include <stdio.h>

// FIXME: Consider removing locks if we're singlethreaded already.
// The static initializer here should work okay, but we want to avoid
// static initialization in general.

namespace npruntime {

// We use StringKey here as the key-type to avoid a string copy to
// construct the map key and for faster comparisons than strcmp.
class StringKey {
public:
  explicit StringKey(const char* str) : string_(str), length_(strlen(str)) { }
  StringKey() : string_(0), length_(0) { }
  explicit StringKey(WTF::HashTableDeletedValueType) :
      string_(HashTableDeletedValue()), length_(0) { }

  StringKey& operator=(const StringKey& other) {
    this->string_ = other.string_;
    this->length_ = other.length_;
    return *this;
  }

  bool IsHashTableDeletedValue() const {
    return string_ == HashTableDeletedValue();
  }

  const char* string_;
  size_t length_;

private:
  const char* HashTableDeletedValue() const {
    return reinterpret_cast<const char*>(-1);
  }
};

inline bool operator==(const StringKey& x, const StringKey& y) {
  if (x.length_ != y.length_)
    return false;
  if (x.string_ == y.string_)
    return true;

  assert(!x.IsHashTableDeletedValue() && !y.IsHashTableDeletedValue());
  return !memcmp(x.string_, y.string_, y.length_);
}

// Implement WTF::DefaultHash<StringKey>::Hash interface.
struct StringKeyHash {
  static unsigned GetHash(const StringKey& key) {
    // Compute string hash.
    unsigned hash = 0;
    size_t len = key.length_;
    const char* str = key.string_;
    for (size_t i = 0; i < len; i++) {
      char c = str[i];
      hash += c;
      hash += (hash << 10);
      hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    if (hash == 0)
      hash = 27;
    return hash;
  }

  static bool Equal(const StringKey& x, const StringKey& y) {
    return x == y;
  }

  static const bool safe_to_compare_to_empty_or_deleted = true;
};

}  // namespace npruntime

using npruntime::StringKey;
using npruntime::StringKeyHash;

// Implement HashTraits<StringKey>
struct StringKeyHashTraits : WTF::GenericHashTraits<StringKey> {
  static void ConstructDeletedValue(StringKey& slot, bool) {
    new (&slot) StringKey(WTF::kHashTableDeletedValue);
  }

  static bool IsDeletedValue(const StringKey& value) {
    return value.IsHashTableDeletedValue();
  }
};

typedef WTF::HashMap<StringKey,
                     blink::PrivateIdentifier*,
                     StringKeyHash,
                     StringKeyHashTraits>
    StringIdentifierMap;

static StringIdentifierMap* GetStringIdentifierMap() {
  static StringIdentifierMap* string_identifier_map = 0;
  if (!string_identifier_map)
    string_identifier_map = new StringIdentifierMap();
  return string_identifier_map;
}

typedef WTF::HashMap<int, blink::PrivateIdentifier*> IntIdentifierMap;

static IntIdentifierMap* GetIntIdentifierMap() {
  static IntIdentifierMap* int_identifier_map = 0;
  if (!int_identifier_map)
    int_identifier_map = new IntIdentifierMap();
  return int_identifier_map;
}

extern "C" {

NPIdentifier _NPN_GetStringIdentifier(const NPUTF8* name) {
  assert(name);

  if (name) {
    StringKey key(name);
    StringIdentifierMap* ident_map = GetStringIdentifierMap();
    StringIdentifierMap::iterator iter = ident_map->find(key);
    if (iter != ident_map->end())
      return static_cast<NPIdentifier>(iter->value);

    size_t name_len = key.length_;

    // We never release identifiers, so this dictionary will grow.
    blink::PrivateIdentifier* identifier =
        static_cast<blink::PrivateIdentifier*>(
            malloc(sizeof(blink::PrivateIdentifier) + name_len + 1));
    char* name_storage = reinterpret_cast<char*>(identifier + 1);
    memcpy(name_storage, name, name_len + 1);
    identifier->is_string = true;
    identifier->value.string = reinterpret_cast<NPUTF8*>(name_storage);
    key.string_ = name_storage;
    ident_map->Set(key, identifier);
    return (NPIdentifier)identifier;
  }

  return 0;
}

void _NPN_GetStringIdentifiers(const NPUTF8** names, int32_t name_count,
     NPIdentifier* identifiers) {
  assert(names);
  assert(identifiers);

  if (names && identifiers) {
    for (int i = 0; i < name_count; i++)
      identifiers[i] = _NPN_GetStringIdentifier(names[i]);
  }
}

NPIdentifier _NPN_GetIntIdentifier(int32_t int_id) {
  // Special case for -1 and 0, both cannot be used as key in HashMap.
  if (!int_id || int_id == -1) {
    static blink::PrivateIdentifier* minus_one_or_zero_ids[2];
    blink::PrivateIdentifier* id = minus_one_or_zero_ids[int_id + 1];
    if (!id) {
      id = reinterpret_cast<blink::PrivateIdentifier*>(
          malloc(sizeof(blink::PrivateIdentifier)));
      id->is_string = false;
      id->value.number = int_id;
      minus_one_or_zero_ids[int_id + 1] = id;
    }
    return (NPIdentifier) id;
  }

  IntIdentifierMap* ident_map = GetIntIdentifierMap();
  IntIdentifierMap::iterator iter = ident_map->find(int_id);
  if (iter != ident_map->end())
    return static_cast<NPIdentifier>(iter->value);

  // We never release identifiers, so this dictionary will grow.
  blink::PrivateIdentifier* identifier =
      reinterpret_cast<blink::PrivateIdentifier*>(
          malloc(sizeof(blink::PrivateIdentifier)));
  identifier->is_string = false;
  identifier->value.number = int_id;
  ident_map->Set(int_id, identifier);
  return (NPIdentifier)identifier;
}

bool _NPN_IdentifierIsString(NPIdentifier identifier) {
  blink::PrivateIdentifier* private_identifier =
      reinterpret_cast<blink::PrivateIdentifier*>(identifier);
  return private_identifier->is_string;
}

NPUTF8 *_NPN_UTF8FromIdentifier(NPIdentifier identifier) {
  blink::PrivateIdentifier* private_identifier =
      reinterpret_cast<blink::PrivateIdentifier*>(identifier);
  if (!private_identifier->is_string || !private_identifier->value.string)
    return 0;

  return (NPUTF8*) strdup(private_identifier->value.string);
}

int32_t _NPN_IntFromIdentifier(NPIdentifier identifier) {
  blink::PrivateIdentifier* private_identifier =
      reinterpret_cast<blink::PrivateIdentifier*>(identifier);
  if (private_identifier->is_string)
    return 0;
  return private_identifier->value.number;
}

void _NPN_ReleaseVariantValue(NPVariant* variant) {
  assert(variant);

  if (variant->type == NPVariantType_Object) {
    _NPN_ReleaseObject(variant->value.objectValue);
    variant->value.objectValue = 0;
  } else if (variant->type == NPVariantType_String) {
    free((void*)variant->value.stringValue.UTF8Characters);
    variant->value.stringValue.UTF8Characters = 0;
    variant->value.stringValue.UTF8Length = 0;
  }

  variant->type = NPVariantType_Void;
}

NPObject *_NPN_CreateObject(NPP npp, NPClass* np_class) {
  assert(np_class);
  VLOG(2) << __PRETTY_FUNCTION__ << ": np_class="
      << static_cast<void*>(np_class);

  if (np_class) {
    NPObject* np_object;
    if (np_class->allocate != 0)
      np_object = np_class->allocate(npp, np_class);
    else
      np_object = reinterpret_cast<NPObject*>(malloc(sizeof(NPObject)));

    np_object->_class = np_class;
    np_object->referenceCount = 1;
    VLOG(2) << __PRETTY_FUNCTION__ << ": np_object="
        << static_cast<void*>(np_object)
        << " np_class=" << static_cast<void*>(np_class);
    return np_object;
  }

  return 0;
}

NPObject* _NPN_RetainObject(NPObject* np_object) {
  assert(np_object);
  assert(np_object->referenceCount > 0);
  VLOG(2) << __PRETTY_FUNCTION__ << ": np_object="
      << static_cast<void*>(np_object);

  if (np_object)
    np_object->referenceCount++;

  return np_object;
}

// _NPN_DeallocateObject actually deletes the object.  Technically,
// callers should use _NPN_ReleaseObject.  Webkit exposes this function
// to kill objects which plugins may not have properly released.
void _NPN_DeallocateObject(NPObject* np_object) {
  assert(np_object);
  VLOG(2) << __PRETTY_FUNCTION__ << ": np_object="
      << static_cast<void*>(np_object);

  if (np_object) {
    // NPObjects that remain in pure C++ may never have wrappers.
    // Hence, if it's not already alive, don't unregister it.
    // If it is alive, unregister it as the *last* thing we do
    // so that it can do as much cleanup as possible on its own.
    if (_NPN_IsAlive(np_object))
      _NPN_UnregisterObject(np_object);

    np_object->referenceCount = 0xFFFFFFFF;
    if (np_object->_class->deallocate)
      np_object->_class->deallocate(np_object);
    else
      free(np_object);
  }
}

void _NPN_ReleaseObject(NPObject* np_object) {
  assert(np_object);
  assert(np_object->referenceCount >= 1);
  VLOG(2) << __PRETTY_FUNCTION__ << ": np_object="
      << static_cast<void*>(np_object);

  if (np_object && np_object->referenceCount >= 1) {
    if (!--np_object->referenceCount)
      _NPN_DeallocateObject(np_object);
  }
}

void _NPN_InitializeVariantWithStringCopy(NPVariant* variant,
    const NPString* value) {
  variant->type = NPVariantType_String;
  variant->value.stringValue.UTF8Length = value->UTF8Length;
  variant->value.stringValue.UTF8Characters =
      reinterpret_cast<NPUTF8*>(malloc(sizeof(NPUTF8) * value->UTF8Length));
  memcpy((void*)variant->value.stringValue.UTF8Characters,
          value->UTF8Characters,
          sizeof(NPUTF8) * value->UTF8Length);
}

}  // extern "C"

// NPN_Registry
//
// The registry is designed for quick lookup of NPObjects.
// JS needs to be able to quickly lookup a given NPObject to determine
// if it is alive or not.
// The browser needs to be able to quickly lookup all NPObjects which are
// "owned" by an object.
//
// The LiveObjectMap is a hash table of all live objects to their owner
// objects.  Presence in this table is used primarily to determine if
// objects are live or not.
//
// The RootObjectMap is a hash table of root objects to a set of
// objects that should be deactivated in sync with the root.  A
// root is defined as a top-level owner object.  This is used on
// LocalFrame teardown to deactivate all objects associated
// with a particular plugin.

typedef WTF::HashSet<NPObject*> NPObjectSet;
typedef WTF::HashMap<NPObject*, NPObject*> NPObjectMap;
typedef WTF::HashMap<NPObject*, NPObjectSet*> NPRootObjectMap;

// A map of live NPObjects with pointers to their Roots.
static NPObjectMap& LiveObjectMap() {
  DEFINE_STATIC_LOCAL(NPObjectMap, object_map, ());
  return object_map;
}

// A map of the root objects and the list of NPObjects
// associated with that object.
static NPRootObjectMap& RootObjectMap() {
  DEFINE_STATIC_LOCAL(NPRootObjectMap, object_map, ());
  return object_map;
}

extern "C" {

void _NPN_RegisterObject(NPObject* np_object, NPObject* owner) {
  assert(np_object);

  // Check if already registered.
  if (LiveObjectMap().find(np_object) != LiveObjectMap().end())
    return;

  if (!owner) {
    // Registering a new owner object.
    assert(RootObjectMap().find(np_object) == RootObjectMap().end());
    RootObjectMap().Set(np_object, new NPObjectSet());
  } else {
    // Always associate this object with it's top-most parent.
    // Since we always flatten, we only have to look up one level.
    auto parent_entry = LiveObjectMap().find(owner);
    NPObject* parent = (parent_entry != LiveObjectMap().end())
      ? parent_entry->value : nullptr;

    if (parent)
      owner = parent;

    assert(RootObjectMap().find(np_object) == RootObjectMap().end());
    auto owner_entry = RootObjectMap().find(owner);
    if (owner_entry != RootObjectMap().end())
      owner_entry->value->insert(np_object);
  }

  assert(LiveObjectMap().find(np_object) == LiveObjectMap().end());
  LiveObjectMap().Set(np_object, owner);
}

void _NPN_UnregisterObject(NPObject* np_object) {
  assert(np_object);
  SECURITY_DCHECK(LiveObjectMap().find(np_object) != LiveObjectMap().end());

  NPObject* owner = (LiveObjectMap().find(np_object) != LiveObjectMap().end())
    ? LiveObjectMap().find(np_object)->value : nullptr;

  VLOG(2) << __PRETTY_FUNCTION__ << ": np_object="
      << static_cast<void*>(np_object) << " owner="
      << static_cast<void*>(owner);

  if (!owner) {
    // Unregistering a owner object; also unregister it's descendants.
    SECURITY_DCHECK(RootObjectMap().find(np_object) != RootObjectMap().end());
    NPObjectSet* set = RootObjectMap().find(np_object)->value;
    while (set->size() > 0) {
      unsigned size = set->size();
      NPObject* sub_object = *(set->begin());
      // The sub-object should not be a owner!
      assert(RootObjectMap().find(sub_object) == RootObjectMap().end());

      VLOG(2) << __PRETTY_FUNCTION__ << ": subobject="
          << static_cast<void*>(sub_object);

      // First, unregister the object.
      set->erase(sub_object);
      LiveObjectMap().erase(sub_object);

      // Script objects hold a reference to their LocalDOMWindow*, which
      // is going away if we're unregistering the associated owner
      // NPObject. Clear it out.
      if (blink::V8NPObject* v8npObject =
              blink::NPObjectToV8NPObject(sub_object))
        v8npObject->root_object = nullptr;

      // Remove the JS references to the object.
      blink::ForgetV8ObjectForNPObject(sub_object);

      auto new_size = set->size();
      DCHECK_LT(new_size, size);
    }
    delete set;
    RootObjectMap().erase(np_object);
  } else {
    auto ownerEntry = RootObjectMap().find(owner);
    if (ownerEntry != RootObjectMap().end()) {
      NPObjectSet* list = ownerEntry->value;
      assert(list->find(np_object) != list->end());
      list->erase(np_object);
    }
  }

  LiveObjectMap().erase(np_object);
  blink::ForgetV8ObjectForNPObject(np_object);
}

bool _NPN_IsAlive(NPObject* np_object) {
  return LiveObjectMap().find(np_object) != LiveObjectMap().end();
}

}  // extern "C"
