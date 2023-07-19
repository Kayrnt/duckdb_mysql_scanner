use crate::{
    duckly::{
        duckdb_validity_row_is_valid, duckdb_validity_set_row_invalid,
        duckdb_validity_set_row_valid, duckdb_validity_set_row_validity, duckdb_vector,
        duckdb_vector_assign_string_element, duckdb_vector_assign_string_element_len,
        duckdb_vector_ensure_validity_writable, duckdb_vector_get_column_type,
        duckdb_vector_get_data, duckdb_vector_get_validity, duckdb_vector_size, idx_t,
        duckdb_list_entry, duckdb_list_vector_get_child, duckdb_list_vector_get_size,
        duckdb_list_vector_reserve, duckdb_list_vector_set_size, duckdb_struct_type_child_count,
        duckdb_struct_type_child_name, duckdb_struct_vector_get_child,
    },
    LogicalType,
};
use std::any::Any;
use std::fmt::Debug;
use std::ffi::CString;
use std::{ffi::c_char, marker::PhantomData, slice};

/// Vector of values of a specified PhysicalType.
pub struct Vector<T>(duckdb_vector, PhantomData<T>);

impl<T> From<duckdb_vector> for Vector<T> {
    fn from(ptr: duckdb_vector) -> Self {
        Self(ptr, PhantomData {})
    }
}

impl<T> Vector<T> {
    /// Retrieves the data pointer of the vector.
    ///
    /// The data pointer can be used to read or write values from the vector. How to read or write values depends on the type of the vector.
    pub fn get_data(&self) -> *mut T {
        unsafe { duckdb_vector_get_data(self.0).cast() }
    }

    /// Assigns a string element in the vector at the specified location.
    ///
    /// # Arguments
    ///  * `index` - The row position in the vector to assign the string to
    ///  * `str` - The string
    ///  * `str_len` - The length of the string (in bytes)
    ///
    /// # Safety
    pub unsafe fn assign_string_element_len(
        &self,
        index: idx_t,
        str_: *const c_char,
        str_len: idx_t,
    ) {
        duckdb_vector_assign_string_element_len(self.0, index, str_, str_len);
    }

    /// Assigns a string element in the vector at the specified location.
    ///
    /// # Arguments
    ///  * `index` - The row position in the vector to assign the string to
    ///  * `str` - The null-terminated string"]
    ///
    /// # Safety
    pub unsafe fn assign_string_element(&self, index: idx_t, str_: *const c_char) {
        duckdb_vector_assign_string_element(self.0, index, str_);
    }

    /// Retrieves the data pointer of the vector as a slice
    ///
    /// The data pointer can be used to read or write values from the vector. How to read or write values depends on the type of the vector.
    pub fn get_data_as_slice(&mut self) -> &mut [T] {
        let ptr = self.get_data();
        unsafe { slice::from_raw_parts_mut(ptr, duckdb_vector_size() as usize) }
    }

    /// Retrieves the column type of the specified vector.
    pub fn get_column_type(&self) -> LogicalType {
        unsafe { LogicalType::from(duckdb_vector_get_column_type(self.0)) }
    }
    /// Retrieves the validity mask pointer of the specified vector.
    ///
    /// If all values are valid, this function MIGHT return NULL!
    ///
    /// The validity mask is a bitset that signifies null-ness within the data chunk. It is a series of uint64_t values, where each uint64_t value contains validity for 64 tuples. The bit is set to 1 if the value is valid (i.e. not NULL) or 0 if the value is invalid (i.e. NULL).
    ///
    /// Validity of a specific value can be obtained like this:
    ///
    /// idx_t entry_idx = row_idx / 64; idx_t idx_in_entry = row_idx % 64; bool is_valid = validity_maskentry_idx & (1 << idx_in_entry);
    ///
    /// Alternatively, the (slower) row_is_valid function can be used.
    ///
    /// returns: The pointer to the validity mask, or NULL if no validity mask is present
    pub fn get_validity(&self) -> ValidityMask {
        unsafe { ValidityMask(duckdb_vector_get_validity(self.0), duckdb_vector_size()) }
    }
    /// Ensures the validity mask is writable by allocating it.
    ///
    /// After this function is called, get_validity will ALWAYS return non-NULL. This allows null values to be written to the vector, regardless of whether a validity mask was present before.
    pub fn ensure_validity_writable(&self) {
        unsafe { duckdb_vector_ensure_validity_writable(self.0) };
    }

    pub fn set_row_validity(&self, idx: idx_t, valid: bool) -> () {
        unsafe {
            duckdb_validity_set_row_validity(
                duckdb_vector_get_validity(self.0),
                idx,
                valid
            )
        };
    }

    pub fn set_row_invalid(&self, idx: idx_t) -> () {
        unsafe {
            duckdb_validity_set_row_invalid(duckdb_vector_get_validity(self.0), idx)
        };
    }

    pub fn set_row_valid(&self, idx: idx_t) -> () {
        unsafe {
            duckdb_validity_set_row_valid(duckdb_vector_get_validity(self.0), idx)
        };
    }
}

pub trait Vector2 {
    fn as_any(&self) -> &dyn Any;

    fn as_mut_any(&mut self) -> &mut dyn Any;
}

pub struct FlatVector {
    ptr: duckdb_vector,
    capacity: usize,
}

impl From<duckdb_vector> for FlatVector {
    fn from(ptr: duckdb_vector) -> Self {
        Self {
            ptr,
            capacity: unsafe { duckdb_vector_size() as usize },
        }
    }
}

impl Vector2 for FlatVector {
    fn as_any(&self) -> &dyn Any {
        self
    }

    fn as_mut_any(&mut self) -> &mut dyn Any {
        self
    }
}

impl FlatVector {
    fn with_capacity(ptr: duckdb_vector, capacity: usize) -> Self {
        Self { ptr, capacity }
    }

    pub fn capacity(&self) -> usize {
        self.capacity
    }

    /// Returns an unsafe mutable pointer to the vector’s
    pub fn as_mut_ptr<T>(&self) -> *mut T {
        unsafe { duckdb_vector_get_data(self.ptr).cast() }
    }

    pub fn as_slice<T>(&self) -> &[T] {
        unsafe { slice::from_raw_parts(self.as_mut_ptr(), self.capacity()) }
    }

    pub fn as_mut_slice<T>(&mut self) -> &mut [T] {
        unsafe { slice::from_raw_parts_mut(self.as_mut_ptr(), self.capacity()) }
    }

    pub fn logical_type(&self) -> LogicalType {
        LogicalType::from(unsafe { duckdb_vector_get_column_type(self.ptr) })
    }

    pub fn copy<T: Copy>(&mut self, data: &[T]) {
        assert!(data.len() <= self.capacity());
        self.as_mut_slice::<T>()[0..data.len()].copy_from_slice(data);
    }
}

pub trait Inserter<T> {
    fn insert(&self, index: usize, value: T);
}

impl Inserter<&str> for FlatVector {
    fn insert(&self, index: usize, value: &str) {
        let cstr = CString::new(value.as_bytes()).unwrap();
        unsafe {
            duckdb_vector_assign_string_element(self.ptr, index as u64, cstr.as_ptr());
        }
    }
}

pub struct ListVector {
    /// ListVector does not own the vector pointer.
    entries: FlatVector,
}

impl From<duckdb_vector> for ListVector {
    fn from(ptr: duckdb_vector) -> Self {
        Self {
            entries: FlatVector::from(ptr),
        }
    }
}

impl ListVector {
    pub fn len(&self) -> usize {
        unsafe { duckdb_list_vector_get_size(self.entries.ptr) as usize }
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    // TODO: not ideal interface. Where should we keep capacity.
    pub fn child(&self, capacity: usize) -> FlatVector {
        self.reserve(capacity);
        FlatVector::with_capacity(
            unsafe { duckdb_list_vector_get_child(self.entries.ptr) },
            capacity,
        )
    }

    /// Set primitive data to the child node.
    pub fn set_child<T: Copy>(&self, data: &[T]) {
        self.child(data.len()).copy(data);
        self.set_len(data.len());
    }

    pub fn set_entry(&mut self, idx: usize, offset: usize, length: usize) {
        self.entries.as_mut_slice::<duckdb_list_entry>()[idx].offset = offset as u64;
        self.entries.as_mut_slice::<duckdb_list_entry>()[idx].length = length as u64;
    }

    /// Reserve the capacity for its child node.
    fn reserve(&self, capacity: usize) {
        unsafe { duckdb_list_vector_reserve(self.entries.ptr, capacity as u64); }
    }

    pub fn set_len(&self, new_len: usize) {
        unsafe { duckdb_list_vector_set_size(self.entries.ptr, new_len as u64); }
    }
}

pub struct StructVector {
    /// ListVector does not own the vector pointer.
    ptr: duckdb_vector,
}

impl From<duckdb_vector> for StructVector {
    fn from(ptr: duckdb_vector) -> Self {
        Self { ptr }
    }
}

impl StructVector {
    pub fn child(&self, idx: usize) -> FlatVector {
        FlatVector::from(unsafe { duckdb_struct_vector_get_child(self.ptr, idx as u64) })
    }

    /// Take the child as [StructVector].
    pub fn struct_vector_child(&self, idx: usize) -> StructVector {
        Self::from(unsafe { duckdb_struct_vector_get_child(self.ptr, idx as u64) })
    }

    pub fn list_vector_child(&self, idx: usize) -> ListVector {
        ListVector::from(unsafe { duckdb_struct_vector_get_child(self.ptr, idx as u64) })
    }

    /// Get the logical type of this struct vector.
    pub fn logical_type(&self) -> LogicalType {
        LogicalType::from(unsafe { duckdb_vector_get_column_type(self.ptr) })
    }

    pub fn child_name(&self, idx: usize) -> String {
        let logical_type = self.logical_type();
        unsafe {
            let child_name_ptr = duckdb_struct_type_child_name(logical_type.typ, idx as u64);
            let c_str = CString::from_raw(child_name_ptr);
            let name = c_str.to_str().unwrap();
            // duckdb_free(child_name_ptr.cast());
            name.to_string()
        }
    }

    pub fn num_children(&self) -> usize {
        let logical_type = self.logical_type();
        unsafe { duckdb_struct_type_child_count(logical_type.typ) as usize }
    }
}

pub struct ValidityMask(*mut u64, idx_t);

impl Debug for ValidityMask {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let base = (0..self.1)
            .map(|row| if self.row_is_valid(row) { "." } else { "X" })
            .collect::<Vec<&str>>()
            .join("");

        f.debug_struct("ValidityMask")
            .field("validity", &base)
            .finish()
    }
}

impl ValidityMask {
    /// Returns whether or not a row is valid (i.e. not NULL) in the given validity mask.
    ///
    /// # Arguments
    ///  * `row`: The row index
    /// returns: true if the row is valid, false otherwise
    pub fn row_is_valid(&self, row: idx_t) -> bool {
        unsafe { duckdb_validity_row_is_valid(self.0, row) }
    }
    /// In a validity mask, sets a specific row to either valid or invalid.
    ///
    /// Note that ensure_validity_writable should be called before calling get_validity, to ensure that there is a validity mask to write to.
    ///
    /// # Arguments
    ///  * `row`: The row index
    ///  * `valid`: Whether or not to set the row to valid, or invalid
    pub fn set_row_validity(&self, row: idx_t, valid: bool) {
        unsafe { duckdb_validity_set_row_validity(self.0, row, valid) }
    }
    /// In a validity mask, sets a specific row to invalid.
    ///
    /// Equivalent to set_row_validity with valid set to false.
    ///
    /// # Arguments
    ///  * `row`: The row index
    pub fn set_row_invalid(&self, row: idx_t) {
        unsafe { duckdb_validity_set_row_invalid(self.0, row) }
    }
    /// In a validity mask, sets a specific row to valid.
    ///
    /// Equivalent to set_row_validity with valid set to true.
    ///
    /// # Arguments
    ///  * `row`: The row index
    pub fn set_row_valid(&self, row: idx_t) {
        unsafe { duckdb_validity_set_row_valid(self.0, row) }
    }
}

#[cfg(test)]
mod test {
    use crate::constants::LogicalTypeId;
    use crate::{DataChunk, LogicalType};

    #[test]
    fn test_vector() {
        let datachunk = DataChunk::new(vec![LogicalType::new(LogicalTypeId::Bigint)]);
        let mut vector = datachunk.get_vector::<u64>(0);
        let data = vector.get_data_as_slice();

        data[0] = 42;
    }
}
