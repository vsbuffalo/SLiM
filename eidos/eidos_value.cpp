//
//  eidos_value.cpp
//  Eidos
//
//  Created by Ben Haller on 4/7/15.
//  Copyright (c) 2015 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
//

//	This file is part of Eidos.
//
//	Eidos is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	Eidos is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with Eidos.  If not, see <http://www.gnu.org/licenses/>.

#include "eidos_value.h"
#include "eidos_functions.h"
#include "eidos_call_signature.h"


using std::string;
using std::vector;
using std::endl;
using std::istringstream;
using std::ostringstream;
using std::istream;
using std::ostream;


//
//	Global static EidosValue objects; these are effectively const, although EidosValues can't be declared as const.
//	Internally, thse are implemented as subclasses that terminate if they are dealloced or modified.
//

EidosValue_NULL *gStaticEidosValueNULL = EidosValue_NULL_const::Static_EidosValue_NULL();
EidosValue_NULL *gStaticEidosValueNULLInvisible = EidosValue_NULL_const::Static_EidosValue_NULL_Invisible();

EidosValue_Logical *gStaticEidosValue_LogicalT = EidosValue_Logical_const::Static_EidosValue_Logical_T();
EidosValue_Logical *gStaticEidosValue_LogicalF = EidosValue_Logical_const::Static_EidosValue_Logical_F();


string StringForEidosValueType(const EidosValueType p_type)
{
	switch (p_type)
	{
		case EidosValueType::kValueNULL:		return gStr_NULL;
		case EidosValueType::kValueLogical:		return gStr_logical;
		case EidosValueType::kValueString:		return gStr_string;
		case EidosValueType::kValueInt:			return gStr_integer;
		case EidosValueType::kValueFloat:		return gStr_float;
		case EidosValueType::kValueObject:		return gStr_object;
	}
}

std::ostream &operator<<(std::ostream &p_outstream, const EidosValueType p_type)
{
	p_outstream << StringForEidosValueType(p_type);
	
	return p_outstream;
}

std::string StringForEidosValueMask(const EidosValueMask p_mask, const std::string &p_name)
{
	std::string out_string;
	bool is_optional = !!(p_mask & kValueMaskOptional);
	bool requires_singleton = !!(p_mask & kValueMaskSingleton);
	EidosValueMask type_mask = p_mask & kValueMaskFlagStrip;
	
	if (is_optional)
		out_string += "[";
	
	if (type_mask == kValueMaskNone)			out_string += "?";
	else if (type_mask == kValueMaskAny)		out_string += "*";
	else if (type_mask == kValueMaskAnyBase)	out_string += "+";
	else if (type_mask == kValueMaskNULL)		out_string += gStr_void;
	else if (type_mask == kValueMaskLogical)	out_string += gStr_logical;
	else if (type_mask == kValueMaskString)		out_string += gStr_string;
	else if (type_mask == kValueMaskInt)		out_string += gStr_integer;
	else if (type_mask == kValueMaskFloat)		out_string += gStr_float;
	else if (type_mask == kValueMaskObject)		out_string += gStr_object;
	else if (type_mask == kValueMaskNumeric)	out_string += gStr_numeric;
	else
	{
		if (type_mask & kValueMaskNULL)			out_string += "N";
		if (type_mask & kValueMaskLogical)		out_string += "l";
		if (type_mask & kValueMaskInt)			out_string += "i";
		if (type_mask & kValueMaskFloat)		out_string += "f";
		if (type_mask & kValueMaskString)		out_string += "s";
		if (type_mask & kValueMaskObject)		out_string += "o";
	}
	
	if (requires_singleton)
		out_string += "$";
	
	if (p_name.length() > 0)
	{
		out_string += " ";
		out_string += p_name;
	}
	
	if (is_optional)
		out_string += "]";
	
	return out_string;
}

// returns -1 if value1[index1] < value2[index2], 0 if ==, 1 if >, with full type promotion
int CompareEidosValues(const EidosValue *p_value1, int p_index1, const EidosValue *p_value2, int p_index2)
{
	EidosValueType type1 = p_value1->Type();
	EidosValueType type2 = p_value2->Type();
	
	if ((type1 == EidosValueType::kValueNULL) || (type2 == EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (CompareEidosValues): comparison with NULL is illegal." << eidos_terminate();
	
	// comparing one object to another is legal, but objects cannot be compared to other types
	if ((type1 == EidosValueType::kValueObject) && (type2 == EidosValueType::kValueObject))
	{
		EidosObjectElement *element1 = p_value1->ObjectElementAtIndex(p_index1);
		EidosObjectElement *element2 = p_value2->ObjectElementAtIndex(p_index2);
		
		return (element1 == element2) ? 0 : -1;		// no relative ordering, just equality comparison; enforced in script_interpreter
	}
	
	// string is the highest type, so we promote to string if either operand is a string
	if ((type1 == EidosValueType::kValueString) || (type2 == EidosValueType::kValueString))
	{
		string string1 = p_value1->StringAtIndex(p_index1);
		string string2 = p_value2->StringAtIndex(p_index2);
		int compare_result = string1.compare(string2);		// not guaranteed to be -1 / 0 / +1, just negative / 0 / positive
		
		return (compare_result < 0) ? -1 : ((compare_result > 0) ? 1 : 0);
	}
	
	// float is the next highest type, so we promote to float if either operand is a float
	if ((type1 == EidosValueType::kValueFloat) || (type2 == EidosValueType::kValueFloat))
	{
		double float1 = p_value1->FloatAtIndex(p_index1);
		double float2 = p_value2->FloatAtIndex(p_index2);
		
		return (float1 < float2) ? -1 : ((float1 > float2) ? 1 : 0);
	}
	
	// int is the next highest type, so we promote to int if either operand is a int
	if ((type1 == EidosValueType::kValueInt) || (type2 == EidosValueType::kValueInt))
	{
		int64_t int1 = p_value1->IntAtIndex(p_index1);
		int64_t int2 = p_value2->IntAtIndex(p_index2);
		
		return (int1 < int2) ? -1 : ((int1 > int2) ? 1 : 0);
	}
	
	// logical is the next highest type, so we promote to logical if either operand is a logical
	if ((type1 == EidosValueType::kValueLogical) || (type2 == EidosValueType::kValueLogical))
	{
		bool logical1 = p_value1->LogicalAtIndex(p_index1);
		bool logical2 = p_value2->LogicalAtIndex(p_index2);
		
		return (logical1 < logical2) ? -1 : ((logical1 > logical2) ? 1 : 0);
	}
	
	// that's the end of the road; we should never reach this point
	EIDOS_TERMINATION << "ERROR (CompareEidosValues): comparison involving type " << type1 << " and type " << type2 << " is undefined." << eidos_terminate();
	return 0;
}


//
//	EidosValue
//
#pragma mark EidosValue

EidosValue::EidosValue(const EidosValue &p_original) : external_temporary_(false), external_permanent_(false), invisible_(false)	// doesn't use original for these flags
{
#pragma unused(p_original)
}

EidosValue::EidosValue(void)
{
}

EidosValue::~EidosValue(void)
{
}

bool EidosValue::LogicalAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR: operand type " << this->Type() << " cannot be converted to type logical." << eidos_terminate();
	return false;
}

std::string EidosValue::StringAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR: operand type " << this->Type() << " cannot be converted to type string." << eidos_terminate();
	return std::string();
}

int64_t EidosValue::IntAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR: operand type " << this->Type() << " cannot be converted to type integer." << eidos_terminate();
	return 0;
}

double EidosValue::FloatAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR: operand type " << this->Type() << " cannot be converted to type float." << eidos_terminate();
	return 0.0;
}

EidosObjectElement *EidosValue::ObjectElementAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR: operand type " << this->Type() << " cannot be converted to type object." << eidos_terminate();
	return nullptr;
}

bool EidosValue::IsMutable(void) const
{
	return true;
}

EidosValue *EidosValue::MutableCopy(void) const
{
	return CopyValues();
}

std::ostream &operator<<(std::ostream &p_outstream, const EidosValue &p_value)
{
	p_value.Print(p_outstream);		// get dynamic dispatch
	
	return p_outstream;
}


//
//	EidosValue_NULL
//
#pragma mark EidosValue_NULL

EidosValue_NULL::EidosValue_NULL(void)
{
}

EidosValue_NULL::~EidosValue_NULL(void)
{
}

EidosValueType EidosValue_NULL::Type(void) const
{
	return EidosValueType::kValueNULL;
}

const std::string *EidosValue_NULL::ElementType(void) const
{
	return &gStr_NULL;
}

int EidosValue_NULL::Count(void) const
{
	return 0;
}

void EidosValue_NULL::Print(std::ostream &p_ostream) const
{
	p_ostream << gStr_NULL;
}

EidosValue *EidosValue_NULL::GetValueAtIndex(const int p_idx) const
{
#pragma unused(p_idx)
	return new EidosValue_NULL;
}

void EidosValue_NULL::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR: operand type " << this->Type() << " does not support setting values with the subscript operator ('[]')." << eidos_terminate();
}

EidosValue *EidosValue_NULL::CopyValues(void) const
{
	return new EidosValue_NULL(*this);
}

EidosValue *EidosValue_NULL::NewMatchingType(void) const
{
	return new EidosValue_NULL;
}

void EidosValue_NULL::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
#pragma unused(p_idx)
	if (p_source_script_value->Type() == EidosValueType::kValueNULL)
		;	// NULL doesn't have values or indices, so this is a no-op
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_NULL::PushValueFromIndexOfEidosValue): type mismatch." << eidos_terminate();
}

void EidosValue_NULL::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	// nothing to do
}


EidosValue_NULL_const::~EidosValue_NULL_const(void)
{
	EIDOS_TERMINATION << "ERROR (EidosValue_NULL_const::~EidosValue_NULL_const): internal error: global constant deallocated." << eidos_terminate();
}

/* static */ EidosValue_NULL *EidosValue_NULL_const::Static_EidosValue_NULL(void)
{
	static EidosValue_NULL_const *static_null = nullptr;
	
	if (!static_null)
	{
		// this is a truly permanent constant object
		static_null = new EidosValue_NULL_const();
		static_null->SetExternalPermanent();
	}
	
	return static_null;
}

/* static */ EidosValue_NULL *EidosValue_NULL_const::Static_EidosValue_NULL_Invisible(void)
{
	static EidosValue_NULL_const *static_null = nullptr;
	
	if (!static_null)
	{
		// this is a truly permanent constant object
		static_null = new EidosValue_NULL_const();
		static_null->invisible_ = true;
		static_null->SetExternalPermanent();
	}
	
	return static_null;
}



//
//	EidosValue_Logical
//
#pragma mark EidosValue_Logical

EidosValue_Logical::EidosValue_Logical(void)
{
}

EidosValue_Logical::EidosValue_Logical(std::vector<bool> &p_boolvec)
{
	values_ = p_boolvec;
}

EidosValue_Logical::EidosValue_Logical(bool p_bool1)
{
	values_.push_back(p_bool1);
}

EidosValue_Logical::EidosValue_Logical(bool p_bool1, bool p_bool2)
{
	values_.push_back(p_bool1);
	values_.push_back(p_bool2);
}

EidosValue_Logical::EidosValue_Logical(bool p_bool1, bool p_bool2, bool p_bool3)
{
	values_.push_back(p_bool1);
	values_.push_back(p_bool2);
	values_.push_back(p_bool3);
}

EidosValue_Logical::EidosValue_Logical(bool p_bool1, bool p_bool2, bool p_bool3, bool p_bool4)
{
	values_.push_back(p_bool1);
	values_.push_back(p_bool2);
	values_.push_back(p_bool3);
	values_.push_back(p_bool4);
}

EidosValue_Logical::EidosValue_Logical(bool p_bool1, bool p_bool2, bool p_bool3, bool p_bool4, bool p_bool5)
{
	values_.push_back(p_bool1);
	values_.push_back(p_bool2);
	values_.push_back(p_bool3);
	values_.push_back(p_bool4);
	values_.push_back(p_bool5);
}

EidosValue_Logical::EidosValue_Logical(bool p_bool1, bool p_bool2, bool p_bool3, bool p_bool4, bool p_bool5, bool p_bool6)
{
	values_.push_back(p_bool1);
	values_.push_back(p_bool2);
	values_.push_back(p_bool3);
	values_.push_back(p_bool4);
	values_.push_back(p_bool5);
	values_.push_back(p_bool6);
}

EidosValue_Logical::~EidosValue_Logical(void)
{
}

EidosValueType EidosValue_Logical::Type(void) const
{
	return EidosValueType::kValueLogical;
}

const std::string *EidosValue_Logical::ElementType(void) const
{
	return &gStr_logical;
}

int EidosValue_Logical::Count(void) const
{
	return (int)values_.size();
}

void EidosValue_Logical::Print(std::ostream &p_ostream) const
{
	if (values_.size() == 0)
	{
		p_ostream << "logical(0)";
	}
	else
	{
		bool first = true;
		
		for (bool value : values_)
		{
			if (first)
				first = false;
			else
				p_ostream << ' ';
			
			p_ostream << (value ? gStr_T : gStr_F);
		}
	}
}

const std::vector<bool> &EidosValue_Logical::LogicalVector(void) const
{
	return values_;
}

bool EidosValue_Logical::LogicalAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

std::string EidosValue_Logical::StringAtIndex(int p_idx) const
{
	return (values_.at(p_idx) ? gStr_T : gStr_F);
}

int64_t EidosValue_Logical::IntAtIndex(int p_idx) const
{
	return (values_.at(p_idx) ? 1 : 0);
}

double EidosValue_Logical::FloatAtIndex(int p_idx) const
{
	return (values_.at(p_idx) ? 1.0 : 0.0);
}

void EidosValue_Logical::PushLogical(bool p_logical)
{
	values_.push_back(p_logical);
}

void EidosValue_Logical::SetLogicalAtIndex(const int p_idx, bool p_logical)
{
	values_.at(p_idx) = p_logical;
}

EidosValue *EidosValue_Logical::GetValueAtIndex(const int p_idx) const
{
	return (values_.at(p_idx) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
}

void EidosValue_Logical::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::SetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate();
	
	values_.at(p_idx) = p_value->LogicalAtIndex(0);
}

EidosValue *EidosValue_Logical::CopyValues(void) const
{
	return new EidosValue_Logical(*this);
}

EidosValue *EidosValue_Logical::NewMatchingType(void) const
{
	return new EidosValue_Logical;
}

void EidosValue_Logical::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
	if (p_source_script_value->Type() == EidosValueType::kValueLogical)
		values_.push_back(p_source_script_value->LogicalAtIndex(p_idx));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::PushValueFromIndexOfEidosValue): type mismatch." << eidos_terminate();
}

void EidosValue_Logical::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_.begin(), values_.end());
	else
		std::sort(values_.begin(), values_.end(), std::greater<bool>());
}


EidosValue_Logical_const::EidosValue_Logical_const(bool p_bool1) : EidosValue_Logical(p_bool1)
{
}

EidosValue_Logical_const::~EidosValue_Logical_const(void)
{
	EIDOS_TERMINATION << "ERROR (EidosValue_NULL_const::~EidosValue_NULL_const): internal error: global constant deallocated." << eidos_terminate();
}

/* static */ EidosValue_Logical *EidosValue_Logical_const::Static_EidosValue_Logical_T(void)
{
	static EidosValue_Logical_const *static_T = nullptr;
	
	if (!static_T)
	{
		// this is a truly permanent constant object
		static_T = new EidosValue_Logical_const(true);
		static_T->SetExternalPermanent();
	}
	
	return static_T;
}

/* static */ EidosValue_Logical *EidosValue_Logical_const::Static_EidosValue_Logical_F(void)
{
	static EidosValue_Logical_const *static_F = nullptr;
	
	if (!static_F)
	{
		// this is a truly permanent constant object
		static_F = new EidosValue_Logical_const(false);
		static_F->SetExternalPermanent();
	}
	
	return static_F;
}

bool EidosValue_Logical_const::IsMutable(void) const
{
	return false;
}

EidosValue *EidosValue_Logical_const::MutableCopy(void) const
{
	return new EidosValue_Logical(*this);	// same as EidosValue_Logical::, but let's not rely on that
}

void EidosValue_Logical_const::PushLogical(bool p_logical)
{
#pragma unused(p_logical)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::PushLogical): internal error: EidosValue_Logical_const is not modifiable." << eidos_terminate();
}

void EidosValue_Logical_const::SetLogicalAtIndex(const int p_idx, bool p_logical)
{
#pragma unused(p_idx, p_logical)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::SetLogicalAtIndex): internal error: EidosValue_Logical_const is not modifiable." << eidos_terminate();
}

void EidosValue_Logical_const::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::SetValueAtIndex): internal error: EidosValue_Logical_const is not modifiable." << eidos_terminate();
}

void EidosValue_Logical_const::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::PushValueFromIndexOfEidosValue): internal error: EidosValue_Logical_const is not modifiable." << eidos_terminate();
}

void EidosValue_Logical_const::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::Sort): internal error: EidosValue_Logical_const is not modifiable." << eidos_terminate();
}


//
//	EidosValue_String
//
#pragma mark EidosValue_String

EidosValue_String::EidosValue_String(void)
{
}

EidosValue_String::EidosValue_String(std::vector<std::string> &p_stringvec)
{
	values_ = p_stringvec;
}

EidosValue_String::EidosValue_String(const std::string &p_string1)
{
	values_.push_back(p_string1);
}

EidosValue_String::EidosValue_String(const std::string &p_string1, const std::string &p_string2)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
}

EidosValue_String::EidosValue_String(const std::string &p_string1, const std::string &p_string2, const std::string &p_string3)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
	values_.push_back(p_string3);
}

EidosValue_String::EidosValue_String(const std::string &p_string1, const std::string &p_string2, const std::string &p_string3, const std::string &p_string4)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
	values_.push_back(p_string3);
	values_.push_back(p_string4);
}

EidosValue_String::EidosValue_String(const std::string &p_string1, const std::string &p_string2, const std::string &p_string3, const std::string &p_string4, const std::string &p_string5)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
	values_.push_back(p_string3);
	values_.push_back(p_string4);
	values_.push_back(p_string5);
}

EidosValue_String::EidosValue_String(const std::string &p_string1, const std::string &p_string2, const std::string &p_string3, const std::string &p_string4, const std::string &p_string5, const std::string &p_string6)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
	values_.push_back(p_string3);
	values_.push_back(p_string4);
	values_.push_back(p_string5);
	values_.push_back(p_string6);
}

EidosValue_String::~EidosValue_String(void)
{
}

EidosValueType EidosValue_String::Type(void) const
{
	return EidosValueType::kValueString;
}

const std::string *EidosValue_String::ElementType(void) const
{
	return &gStr_string;
}

int EidosValue_String::Count(void) const
{
	return (int)values_.size();
}

void EidosValue_String::Print(std::ostream &p_ostream) const
{
	if (values_.size() == 0)
	{
		p_ostream << "string(0)";
	}
	else
	{
		bool first = true;
		
		for (const string &value : values_)
		{
			if (first)
				first = false;
			else
				p_ostream << ' ';
			
			p_ostream << '"' << value << '"';
		}
	}
}

const std::vector<std::string> &EidosValue_String::StringVector(void) const
{
	return values_;
}

bool EidosValue_String::LogicalAtIndex(int p_idx) const
{
	return (values_.at(p_idx).length() > 0);
}

std::string EidosValue_String::StringAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

int64_t EidosValue_String::IntAtIndex(int p_idx) const
{
	const string &index_str = values_.at(p_idx);
	
	return strtoll(index_str.c_str(), nullptr, 10);
}

double EidosValue_String::FloatAtIndex(int p_idx) const
{
	const string &index_str = values_.at(p_idx);
	
	return strtod(index_str.c_str(), nullptr);
}

void EidosValue_String::PushString(const std::string &p_string)
{
	values_.push_back(p_string);
}

EidosValue *EidosValue_String::GetValueAtIndex(const int p_idx) const
{
	return new EidosValue_String(values_.at(p_idx));
}

void EidosValue_String::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String::SetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate();
	
	values_.at(p_idx) = p_value->StringAtIndex(0);
}

EidosValue *EidosValue_String::CopyValues(void) const
{
	return new EidosValue_String(*this);
}

EidosValue *EidosValue_String::NewMatchingType(void) const
{
	return new EidosValue_String;
}

void EidosValue_String::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
	if (p_source_script_value->Type() == EidosValueType::kValueString)
		values_.push_back(p_source_script_value->StringAtIndex(p_idx));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_String::PushValueFromIndexOfEidosValue): type mismatch." << eidos_terminate();
}

void EidosValue_String::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_.begin(), values_.end());
	else
		std::sort(values_.begin(), values_.end(), std::greater<std::string>());
}


//
//	EidosValue_Int
//
#pragma mark EidosValue_Int

EidosValue_Int::~EidosValue_Int(void)
{
}

EidosValueType EidosValue_Int::Type(void) const
{
	return EidosValueType::kValueInt;
}

const std::string *EidosValue_Int::ElementType(void) const
{
	return &gStr_integer;
}

EidosValue *EidosValue_Int::NewMatchingType(void) const
{
	return new EidosValue_Int_vector;
}


// EidosValue_Int_vector

EidosValue_Int_vector::EidosValue_Int_vector(void)
{
}

EidosValue_Int_vector::EidosValue_Int_vector(std::vector<int> &p_intvec)
{
	for (auto intval : p_intvec)
		values_.push_back(intval);
}

EidosValue_Int_vector::EidosValue_Int_vector(std::vector<int64_t> &p_intvec)
{
	values_ = p_intvec;
}

EidosValue_Int_vector::EidosValue_Int_vector(int64_t p_int1, int64_t p_int2)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
}

EidosValue_Int_vector::EidosValue_Int_vector(int64_t p_int1, int64_t p_int2, int64_t p_int3)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
	values_.push_back(p_int3);
}

EidosValue_Int_vector::EidosValue_Int_vector(int64_t p_int1, int64_t p_int2, int64_t p_int3, int64_t p_int4)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
	values_.push_back(p_int3);
	values_.push_back(p_int4);
}

EidosValue_Int_vector::EidosValue_Int_vector(int64_t p_int1, int64_t p_int2, int64_t p_int3, int64_t p_int4, int64_t p_int5)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
	values_.push_back(p_int3);
	values_.push_back(p_int4);
	values_.push_back(p_int5);
}

EidosValue_Int_vector::EidosValue_Int_vector(int64_t p_int1, int64_t p_int2, int64_t p_int3, int64_t p_int4, int64_t p_int5, int64_t p_int6)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
	values_.push_back(p_int3);
	values_.push_back(p_int4);
	values_.push_back(p_int5);
	values_.push_back(p_int6);
}

EidosValue_Int_vector::~EidosValue_Int_vector(void)
{
}

int EidosValue_Int_vector::Count(void) const
{
	return (int)values_.size();
}

void EidosValue_Int_vector::Print(std::ostream &p_ostream) const
{
	if (values_.size() == 0)
	{
		p_ostream << "integer(0)";
	}
	else
	{
		bool first = true;
		
		for (int64_t value : values_)
		{
			if (first)
				first = false;
			else
				p_ostream << ' ';
			
			p_ostream << value;
		}
	}
}

const std::vector<int64_t> &EidosValue_Int_vector::IntVector(void) const
{
	return values_;
}

bool EidosValue_Int_vector::LogicalAtIndex(int p_idx) const
{
	return (values_.at(p_idx) == 0 ? false : true);
}

std::string EidosValue_Int_vector::StringAtIndex(int p_idx) const
{
	// with C++11, could use std::to_string(values_.at(p_idx))
	ostringstream ss;
	
	ss << values_.at(p_idx);
	
	return ss.str();
}

int64_t EidosValue_Int_vector::IntAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

double EidosValue_Int_vector::FloatAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

void EidosValue_Int_vector::PushInt(int64_t p_int)
{
	values_.push_back(p_int);
}

EidosValue *EidosValue_Int_vector::GetValueAtIndex(const int p_idx) const
{
	return new EidosValue_Int_singleton_const(values_.at(p_idx));
}

void EidosValue_Int_vector::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_vector::SetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate();
	
	values_.at(p_idx) = p_value->IntAtIndex(0);
}

EidosValue *EidosValue_Int_vector::CopyValues(void) const
{
	return new EidosValue_Int_vector(*this);
}

void EidosValue_Int_vector::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
	if (p_source_script_value->Type() == EidosValueType::kValueInt)
		values_.push_back(p_source_script_value->IntAtIndex(p_idx));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_vector::PushValueFromIndexOfEidosValue): type mismatch." << eidos_terminate();
}

void EidosValue_Int_vector::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_.begin(), values_.end());
	else
		std::sort(values_.begin(), values_.end(), std::greater<int64_t>());
}



// EidosValue_Int_singleton_const

EidosValue_Int_singleton_const::EidosValue_Int_singleton_const(int64_t p_int1) : value_(p_int1)
{
}

EidosValue_Int_singleton_const::~EidosValue_Int_singleton_const(void)
{
}

int EidosValue_Int_singleton_const::Count(void) const
{
	return 1;
}

void EidosValue_Int_singleton_const::Print(std::ostream &p_ostream) const
{
	p_ostream << value_;
}

bool EidosValue_Int_singleton_const::LogicalAtIndex(int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return (value_ == 0 ? false : true);
}

std::string EidosValue_Int_singleton_const::StringAtIndex(int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	// with C++11, could use std::to_string(values_.at(p_idx))
	ostringstream ss;
	
	ss << value_;
	
	return ss.str();
}

int64_t EidosValue_Int_singleton_const::IntAtIndex(int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return value_;
}

double EidosValue_Int_singleton_const::FloatAtIndex(int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return value_;
}

EidosValue *EidosValue_Int_singleton_const::GetValueAtIndex(const int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return new EidosValue_Int_singleton_const(value_);
}

EidosValue *EidosValue_Int_singleton_const::CopyValues(void) const
{
	return new EidosValue_Int_singleton_const(value_);
}

bool EidosValue_Int_singleton_const::IsMutable(void) const
{
	return false;
}

EidosValue *EidosValue_Int_singleton_const::MutableCopy(void) const
{
	EidosValue_Int_vector *new_vec = new EidosValue_Int_vector();
	
	new_vec->PushInt(value_);
	
	return new_vec;
}

void EidosValue_Int_singleton_const::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton_const::SetValueAtIndex): internal error: EidosValue_Float_singleton_const is not modifiable." << eidos_terminate();
}

void EidosValue_Int_singleton_const::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton_const::PushValueFromIndexOfEidosValue): internal error: EidosValue_Float_singleton_const is not modifiable." << eidos_terminate();
}

void EidosValue_Int_singleton_const::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton_const::Sort): internal error: EidosValue_Float_singleton_const is not modifiable." << eidos_terminate();
}


//
//	EidosValue_Float
//
#pragma mark EidosValue_Float

EidosValue_Float::~EidosValue_Float(void)
{
}

EidosValueType EidosValue_Float::Type(void) const
{
	return EidosValueType::kValueFloat;
}

const std::string *EidosValue_Float::ElementType(void) const
{
	return &gStr_float;
}

EidosValue *EidosValue_Float::NewMatchingType(void) const
{
	return new EidosValue_Float_vector;
}


// EidosValue_Float_vector

EidosValue_Float_vector::EidosValue_Float_vector(void)
{
}

EidosValue_Float_vector::EidosValue_Float_vector(std::vector<double> &p_doublevec)
{
	values_ = p_doublevec;
}

EidosValue_Float_vector::EidosValue_Float_vector(double *p_doublebuf, int p_buffer_length)
{
	for (int index = 0; index < p_buffer_length; index++)
		values_.push_back(p_doublebuf[index]);
}

EidosValue_Float_vector::EidosValue_Float_vector(double p_float1, double p_float2)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
}

EidosValue_Float_vector::EidosValue_Float_vector(double p_float1, double p_float2, double p_float3)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
	values_.push_back(p_float3);
}

EidosValue_Float_vector::EidosValue_Float_vector(double p_float1, double p_float2, double p_float3, double p_float4)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
	values_.push_back(p_float3);
	values_.push_back(p_float4);
}

EidosValue_Float_vector::EidosValue_Float_vector(double p_float1, double p_float2, double p_float3, double p_float4, double p_float5)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
	values_.push_back(p_float3);
	values_.push_back(p_float4);
	values_.push_back(p_float5);
}

EidosValue_Float_vector::EidosValue_Float_vector(double p_float1, double p_float2, double p_float3, double p_float4, double p_float5, double p_float6)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
	values_.push_back(p_float3);
	values_.push_back(p_float4);
	values_.push_back(p_float5);
	values_.push_back(p_float6);
}

EidosValue_Float_vector::~EidosValue_Float_vector(void)
{
}

int EidosValue_Float_vector::Count(void) const
{
	return (int)values_.size();
}

void EidosValue_Float_vector::Print(std::ostream &p_ostream) const
{
	if (values_.size() == 0)
	{
		p_ostream << "float(0)";
	}
	else
	{
		bool first = true;
		
		for (double value : values_)
		{
			if (first)
				first = false;
			else
				p_ostream << ' ';
			
			p_ostream << value;
		}
	}
}

const std::vector<double> &EidosValue_Float_vector::FloatVector(void) const
{
	return values_;
}

bool EidosValue_Float_vector::LogicalAtIndex(int p_idx) const
{
	return (values_.at(p_idx) == 0 ? false : true);
}

std::string EidosValue_Float_vector::StringAtIndex(int p_idx) const
{
	// with C++11, could use std::to_string(values_.at(p_idx))
	ostringstream ss;
	
	ss << values_.at(p_idx);
	
	return ss.str();
}

int64_t EidosValue_Float_vector::IntAtIndex(int p_idx) const
{
	return static_cast<int64_t>(values_.at(p_idx));
}

double EidosValue_Float_vector::FloatAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

void EidosValue_Float_vector::PushFloat(double p_float)
{
	values_.push_back(p_float);
}

EidosValue *EidosValue_Float_vector::GetValueAtIndex(const int p_idx) const
{
	return new EidosValue_Float_singleton_const(values_.at(p_idx));
}

void EidosValue_Float_vector::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::SetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate();
	
	values_.at(p_idx) = p_value->FloatAtIndex(0);
}

EidosValue *EidosValue_Float_vector::CopyValues(void) const
{
	return new EidosValue_Float_vector(*this);
}

void EidosValue_Float_vector::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
	if (p_source_script_value->Type() == EidosValueType::kValueFloat)
		values_.push_back(p_source_script_value->FloatAtIndex(p_idx));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::PushValueFromIndexOfEidosValue): type mismatch." << eidos_terminate();
}

void EidosValue_Float_vector::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_.begin(), values_.end());
	else
		std::sort(values_.begin(), values_.end(), std::greater<double>());
}


// EidosValue_Float_singleton_const

EidosValue_Float_singleton_const::EidosValue_Float_singleton_const(double p_float1) : value_(p_float1)
{
}

EidosValue_Float_singleton_const::~EidosValue_Float_singleton_const(void)
{
}

int EidosValue_Float_singleton_const::Count(void) const
{
	return 1;
}

void EidosValue_Float_singleton_const::Print(std::ostream &p_ostream) const
{
	p_ostream << value_;
}

bool EidosValue_Float_singleton_const::LogicalAtIndex(int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return (value_ == 0 ? false : true);
}

std::string EidosValue_Float_singleton_const::StringAtIndex(int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	// with C++11, could use std::to_string(values_.at(p_idx))
	ostringstream ss;
	
	ss << value_;
	
	return ss.str();
}

int64_t EidosValue_Float_singleton_const::IntAtIndex(int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return static_cast<int64_t>(value_);
}

double EidosValue_Float_singleton_const::FloatAtIndex(int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return value_;
}

EidosValue *EidosValue_Float_singleton_const::GetValueAtIndex(const int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return new EidosValue_Float_singleton_const(value_);
}

EidosValue *EidosValue_Float_singleton_const::CopyValues(void) const
{
	return new EidosValue_Float_singleton_const(value_);
}

bool EidosValue_Float_singleton_const::IsMutable(void) const
{
	return false;
}

EidosValue *EidosValue_Float_singleton_const::MutableCopy(void) const
{
	EidosValue_Float_vector *new_vec = new EidosValue_Float_vector();
	
	new_vec->PushFloat(value_);
	
	return new_vec;
}

void EidosValue_Float_singleton_const::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::SetValueAtIndex): internal error: EidosValue_Float_singleton_const is not modifiable." << eidos_terminate();
}

void EidosValue_Float_singleton_const::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::PushValueFromIndexOfEidosValue): internal error: EidosValue_Float_singleton_const is not modifiable." << eidos_terminate();
}

void EidosValue_Float_singleton_const::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::Sort): internal error: EidosValue_Float_singleton_const is not modifiable." << eidos_terminate();
}


//
//	EidosValue_Object
//
#pragma mark EidosValue_Object

EidosValue_Object::~EidosValue_Object(void)
{
}

EidosValueType EidosValue_Object::Type(void) const
{
	return EidosValueType::kValueObject;
}

EidosValue *EidosValue_Object::NewMatchingType(void) const
{
	return new EidosValue_Object_vector;
}

void EidosValue_Object::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::Sort): Sort() is not defined for type object." << eidos_terminate();
}


// EidosValue_Object_vector

EidosValue_Object_vector::EidosValue_Object_vector(const EidosValue_Object_vector &p_original)
{
	for (auto value : p_original.values_)
		values_.push_back(value->Retain());
}

EidosValue_Object_vector::EidosValue_Object_vector(void)
{
}

EidosValue_Object_vector::EidosValue_Object_vector(std::vector<EidosObjectElement *> &p_elementvec)
{
	values_ = p_elementvec;		// FIXME should this retain?
}

EidosValue_Object_vector::~EidosValue_Object_vector(void)
{
	if (values_.size() != 0)
	{
		for (auto value : values_)
			value->Release();
	}
}

const std::string *EidosValue_Object_vector::ElementType(void) const
{
	if (values_.size() == 0)
		return &gStr_undefined;		// this is relied upon by the type-check machinery
	else
		return values_[0]->ElementType();
}

int EidosValue_Object_vector::Count(void) const
{
	return (int)values_.size();
}

void EidosValue_Object_vector::Print(std::ostream &p_ostream) const
{
	if (values_.size() == 0)
	{
		p_ostream << "object(0)";
	}
	else
	{
		bool first = true;
		
		for (EidosObjectElement *value : values_)
		{
			if (first)
				first = false;
			else
				p_ostream << ' ';
			
			p_ostream << *value;
		}
	}
}

EidosObjectElement *EidosValue_Object_vector::ObjectElementAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

void EidosValue_Object_vector::PushElement(EidosObjectElement *p_element)
{
	if ((values_.size() > 0) && (ElementType() != p_element->ElementType()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::PushElement): the type of an object cannot be changed." << eidos_terminate();
	else
		values_.push_back(p_element->Retain());
}

EidosValue *EidosValue_Object_vector::GetValueAtIndex(const int p_idx) const
{
	return new EidosValue_Object_singleton_const(values_.at(p_idx));
}

void EidosValue_Object_vector::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate();
	
	// can't change the type of element object we collect
	if ((values_.size() > 0) && (ElementType() != p_value->ObjectElementAtIndex(0)->ElementType()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetValueAtIndex): the type of an object cannot be changed." << eidos_terminate();
	
	values_.at(p_idx)->Release();
	values_.at(p_idx) = p_value->ObjectElementAtIndex(0)->Retain();
}

EidosValue *EidosValue_Object_vector::CopyValues(void) const
{
	return new EidosValue_Object_vector(*this);
}

void EidosValue_Object_vector::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
	if (p_source_script_value->Type() == EidosValueType::kValueObject)
	{
		if ((values_.size() > 0) && (ElementType() != p_source_script_value->ObjectElementAtIndex(p_idx)->ElementType()))
			EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::PushValueFromIndexOfEidosValue): the type of an object cannot be changed." << eidos_terminate();
		else
			values_.push_back(p_source_script_value->ObjectElementAtIndex(p_idx)->Retain());
	}
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::PushValueFromIndexOfEidosValue): type mismatch." << eidos_terminate();
}

bool CompareLogicalObjectSortPairsAscending(std::pair<bool, EidosObjectElement*> i, std::pair<bool, EidosObjectElement*> j);
bool CompareLogicalObjectSortPairsAscending(std::pair<bool, EidosObjectElement*> i, std::pair<bool, EidosObjectElement*> j)					{ return (i.first < j.first); }
bool CompareLogicalObjectSortPairsDescending(std::pair<bool, EidosObjectElement*> i, std::pair<bool, EidosObjectElement*> j);
bool CompareLogicalObjectSortPairsDescending(std::pair<bool, EidosObjectElement*> i, std::pair<bool, EidosObjectElement*> j)					{ return (i.first > j.first); }

bool CompareIntObjectSortPairsAscending(std::pair<int64_t, EidosObjectElement*> i, std::pair<int64_t, EidosObjectElement*> j);
bool CompareIntObjectSortPairsAscending(std::pair<int64_t, EidosObjectElement*> i, std::pair<int64_t, EidosObjectElement*> j)					{ return (i.first < j.first); }
bool CompareIntObjectSortPairsDescending(std::pair<int64_t, EidosObjectElement*> i, std::pair<int64_t, EidosObjectElement*> j);
bool CompareIntObjectSortPairsDescending(std::pair<int64_t, EidosObjectElement*> i, std::pair<int64_t, EidosObjectElement*> j)				{ return (i.first > j.first); }

bool CompareFloatObjectSortPairsAscending(std::pair<double, EidosObjectElement*> i, std::pair<double, EidosObjectElement*> j);
bool CompareFloatObjectSortPairsAscending(std::pair<double, EidosObjectElement*> i, std::pair<double, EidosObjectElement*> j)					{ return (i.first < j.first); }
bool CompareFloatObjectSortPairsDescending(std::pair<double, EidosObjectElement*> i, std::pair<double, EidosObjectElement*> j);
bool CompareFloatObjectSortPairsDescending(std::pair<double, EidosObjectElement*> i, std::pair<double, EidosObjectElement*> j)				{ return (i.first > j.first); }

bool CompareStringObjectSortPairsAscending(std::pair<std::string, EidosObjectElement*> i, std::pair<std::string, EidosObjectElement*> j);
bool CompareStringObjectSortPairsAscending(std::pair<std::string, EidosObjectElement*> i, std::pair<std::string, EidosObjectElement*> j)		{ return (i.first < j.first); }
bool CompareStringObjectSortPairsDescending(std::pair<std::string, EidosObjectElement*> i, std::pair<std::string, EidosObjectElement*> j);
bool CompareStringObjectSortPairsDescending(std::pair<std::string, EidosObjectElement*> i, std::pair<std::string, EidosObjectElement*> j)		{ return (i.first > j.first); }

void EidosValue_Object_vector::SortBy(const std::string &p_property, bool p_ascending)
{
	// length 0 is already sorted
	if (values_.size() == 0)
		return;
	
	// figure out what type the property returns
	EidosGlobalStringID property_string_id = EidosGlobalStringIDForString(p_property);
	EidosValue *first_result = values_[0]->GetValueForMember(property_string_id);
	EidosValueType property_type = first_result->Type();
	
	if (first_result->IsTemporary()) delete first_result;
	
	// switch on the property type for efficiency
	switch (property_type)
	{
		case EidosValueType::kValueNULL:
		case EidosValueType::kValueObject:
			EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " returned " << property_type << "; a property that evaluates to logical, int, float, or string is required." << eidos_terminate();
			break;
			
		case EidosValueType::kValueLogical:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			vector<std::pair<bool, EidosObjectElement*>> sortable_pairs;
			
			for (auto value : values_)
			{
				EidosValue *temp_result = value->GetValueForMember(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << eidos_terminate();
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << eidos_terminate();
				
				sortable_pairs.push_back(std::pair<bool, EidosObjectElement*>(temp_result->LogicalAtIndex(0), value));
				
				if (temp_result->IsTemporary()) delete temp_result;
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareLogicalObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareLogicalObjectSortPairsDescending);
			
			// read out our new element vector
			values_.clear();
			
			for (auto sorted_pair : sortable_pairs)
				values_.push_back(sorted_pair.second);
			
			break;
		}
			
		case EidosValueType::kValueInt:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			vector<std::pair<int64_t, EidosObjectElement*>> sortable_pairs;
			
			for (auto value : values_)
			{
				EidosValue *temp_result = value->GetValueForMember(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << eidos_terminate();
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << eidos_terminate();
				
				sortable_pairs.push_back(std::pair<int64_t, EidosObjectElement*>(temp_result->IntAtIndex(0), value));
				
				if (temp_result->IsTemporary()) delete temp_result;
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareIntObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareIntObjectSortPairsDescending);
			
			// read out our new element vector
			values_.clear();
			
			for (auto sorted_pair : sortable_pairs)
				values_.push_back(sorted_pair.second);
			
			break;
		}
			
		case EidosValueType::kValueFloat:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			vector<std::pair<double, EidosObjectElement*>> sortable_pairs;
			
			for (auto value : values_)
			{
				EidosValue *temp_result = value->GetValueForMember(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << eidos_terminate();
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << eidos_terminate();
				
				sortable_pairs.push_back(std::pair<double, EidosObjectElement*>(temp_result->FloatAtIndex(0), value));
				
				if (temp_result->IsTemporary()) delete temp_result;
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareFloatObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareFloatObjectSortPairsDescending);
			
			// read out our new element vector
			values_.clear();
			
			for (auto sorted_pair : sortable_pairs)
				values_.push_back(sorted_pair.second);
			
			break;
		}
			
		case EidosValueType::kValueString:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			vector<std::pair<std::string, EidosObjectElement*>> sortable_pairs;
			
			for (auto value : values_)
			{
				EidosValue *temp_result = value->GetValueForMember(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << eidos_terminate();
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << eidos_terminate();
				
				sortable_pairs.push_back(std::pair<std::string, EidosObjectElement*>(temp_result->StringAtIndex(0), value));
				
				if (temp_result->IsTemporary()) delete temp_result;
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareStringObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareStringObjectSortPairsDescending);
			
			// read out our new element vector
			values_.clear();
			
			for (auto sorted_pair : sortable_pairs)
				values_.push_back(sorted_pair.second);
			
			break;
		}
	}
}

std::vector<std::string> EidosValue_Object_vector::ReadOnlyMembersOfElements(void) const
{
	if (values_.size() == 0)
		return std::vector<std::string>();
	else
		return values_[0]->ReadOnlyMembers();
}

std::vector<std::string> EidosValue_Object_vector::ReadWriteMembersOfElements(void) const
{
	if (values_.size() == 0)
		return std::vector<std::string>();
	else
		return values_[0]->ReadWriteMembers();
}

EidosValue *EidosValue_Object_vector::GetValueForMemberOfElements(EidosGlobalStringID p_member_id) const
{
	auto values_size = values_.size();
	
	if (values_size == 0)
	{
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::GetValueForMemberOfElements): unrecognized member name \"" << StringForEidosGlobalStringID(p_member_id) << "\" (no elements, thus no element type defined)." << eidos_terminate();
		
		return gStaticEidosValueNULLInvisible;
	}
	else if (values_size == 1)
	{
		// the singleton case is very common, so it should be special-cased for speed
		EidosObjectElement *value = values_[0];
		EidosValue *result = value->GetValueForMember(p_member_id);
		
		if (result->Count() != 1)
		{
			// We need to check that this property is const; if not, it is required to give a singleton return
			if (!values_[0]->MemberIsReadOnly(p_member_id))
				EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::GetValueForMemberOfElements): internal error: non-const member " << StringForEidosGlobalStringID(p_member_id) << " produced " << result->Count() << " values for a single element." << eidos_terminate();
		}
		
		return result;
	}
	else
	{
		// get the value from all members and collect the results
		vector<EidosValue*> results;
		bool checked_const_multivalued = false;
		
		for (auto value : values_)
		{
			EidosValue *temp_result = value->GetValueForMember(p_member_id);
			
			if (!checked_const_multivalued && (temp_result->Count() != 1))
			{
				// We need to check that this property is const; if not, it is required to give a singleton return
				if (!values_[0]->MemberIsReadOnly(p_member_id))
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::GetValueForMemberOfElements): internal error: non-const member " << StringForEidosGlobalStringID(p_member_id) << " produced " << temp_result->Count() << " values for a single element." << eidos_terminate();
				
				checked_const_multivalued = true;
			}
			
			results.push_back(temp_result);
		}
		
		// concatenate the results using ConcatenateEidosValues(); we pass our own name as p_function_name, which just makes errors be in our name
		EidosValue *result = ConcatenateEidosValues(gStr_GetValueForMemberOfElements, results.data(), (int)results.size());
		
		// Now we just need to dispose of our temporary EidosValues
		for (EidosValue *temp_value : results)
			if (temp_value->IsTemporary()) delete temp_value;
		
		return result;
	}
}

// This somewhat odd method returns one "representative" EidosValue for the given property, by calling the first element in the
// object.  This is used by code completion to follow the chain of object types along a key path; we don't need all of the values
// that the property would return, we just need one representative value of the proper type.  This is more efficient, of course;
// but the main reason that we don't just call GetValueForMemberOfElements() is that we need an API that will not raise.
EidosValue *EidosValue_Object_vector::GetRepresentativeValueOrNullForMemberOfElements(EidosGlobalStringID p_member_id) const
{
	if (values_.size())
	{
		// check that the member is defined before we call our elements
		const std::string &member_name = StringForEidosGlobalStringID(p_member_id);
		std::vector<std::string> constant_members = values_[0]->ReadOnlyMembers();
		
		if (std::find(constant_members.begin(), constant_members.end(), member_name) == constant_members.end())
		{
			std::vector<std::string> variable_members = values_[0]->ReadWriteMembers();
			
			if (std::find(variable_members.begin(), variable_members.end(), member_name) == variable_members.end())
				return nullptr;
		}
		
		// get a value from the first element and return it; we only need to return one representative value
		return values_[0]->GetValueForMember(p_member_id);
	}
	
	return nullptr;
}

void EidosValue_Object_vector::SetValueForMemberOfElements(EidosGlobalStringID p_member_id, EidosValue *p_value)
{
	if (values_.size() == 0)
	{
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetValueForMemberOfElements): unrecognized member name \"" << StringForEidosGlobalStringID(p_member_id) << "\" (no elements, thus no element type defined)." << eidos_terminate();
	}
	else
	{
		int p_value_count = p_value->Count();
		
		if (p_value_count == 1)
		{
			// we have a multiplex assignment of one value to (maybe) more than one element: x.foo = 10
			for (auto value : values_)
				value->SetValueForMember(p_member_id, p_value);
		}
		else if (p_value_count == Count())
		{
			// we have a one-to-one assignment of values to elements: x.foo = 1:5 (where x has 5 elements)
			for (int value_idx = 0; value_idx < p_value_count; value_idx++)
			{
				EidosValue *temp_rvalue = p_value->GetValueAtIndex(value_idx);
				
				values_[value_idx]->SetValueForMember(p_member_id, temp_rvalue);
				
				if (temp_rvalue->IsTemporary()) delete temp_rvalue;
			}
		}
		else
			EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetValueForMemberOfElements): assignment to a member requires an rvalue that is a singleton (multiplex assignment) or that has a .size() matching the .size of the lvalue." << eidos_terminate();
	}
}

std::vector<std::string> EidosValue_Object_vector::MethodsOfElements(void) const
{
	if (values_.size() == 0)
		return std::vector<std::string>();
	else
		return values_[0]->Methods();
}

const EidosMethodSignature *EidosValue_Object_vector::SignatureForMethodOfElements(EidosGlobalStringID p_method_id) const
{
	if (values_.size() == 0)
	{
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SignatureForMethodOfElements): unrecognized method name " << StringForEidosGlobalStringID(p_method_id) << "." << eidos_terminate();
		
		return new EidosInstanceMethodSignature(gStr_empty_string, kValueMaskNULL);
	}
	else
		return values_[0]->SignatureForMethod(p_method_id);
}

EidosValue *EidosValue_Object_vector::ExecuteClassMethodOfElements(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	if (values_.size() == 0)
	{
		// FIXME perhaps EidosValue_Object_vector should know its element type even when empty, so class methods can be called with no elements?
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::ExecuteClassMethodOfElements): unrecognized class method name " << StringForEidosGlobalStringID(p_method_id) << "." << eidos_terminate();
		
		return gStaticEidosValueNULLInvisible;
	}
	else
	{
		// call the method on one member only, since it is a class method
		EidosValue* result = values_[0]->ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
		
		return result;
	}
}

EidosValue *EidosValue_Object_vector::ExecuteInstanceMethodOfElements(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	auto values_size = values_.size();
	
	if (values_size == 0)
	{
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::ExecuteInstanceMethodOfElements): unrecognized instance method name " << StringForEidosGlobalStringID(p_method_id) << "." << eidos_terminate();
		
		return gStaticEidosValueNULLInvisible;
	}
	else if (values_size == 1)
	{
		// the singleton case is very common, so it should be special-cased for speed
		EidosObjectElement *value = values_[0];
		EidosValue *result = value->ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
		
		return result;
	}
	else
	{
		// call the method on all members and collect the results
		vector<EidosValue*> results;
		
		for (auto value : values_)
			results.push_back(value->ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter));
		
		// concatenate the results using ConcatenateEidosValues(); we pass our own name as p_function_name, which just makes errors be in our name
		EidosValue *result = ConcatenateEidosValues(gStr_ExecuteMethod, results.data(), (int)results.size());
		
		// Now we just need to dispose of our temporary EidosValues
		for (EidosValue *temp_value : results)
			if (temp_value->IsTemporary()) delete temp_value;
		
		return result;
	}
}


// EidosValue_Object_singleton_const

EidosValue_Object_singleton_const::EidosValue_Object_singleton_const(EidosObjectElement *p_element1) : value_(p_element1)
{
	p_element1->Retain();
}

EidosValue_Object_singleton_const::~EidosValue_Object_singleton_const(void)
{
	value_->Release();
}

const std::string *EidosValue_Object_singleton_const::ElementType(void) const
{
	return value_->ElementType();
}

int EidosValue_Object_singleton_const::Count(void) const
{
	return 1;
}

void EidosValue_Object_singleton_const::Print(std::ostream &p_ostream) const
{
	p_ostream << *value_;
}

EidosObjectElement *EidosValue_Object_singleton_const::ObjectElementAtIndex(int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton_const::ObjectElementAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return value_;
}

EidosValue *EidosValue_Object_singleton_const::GetValueAtIndex(const int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton_const::GetValueAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return new EidosValue_Object_singleton_const(value_);
}

EidosValue *EidosValue_Object_singleton_const::CopyValues(void) const
{
	return new EidosValue_Object_singleton_const(value_);
}

bool EidosValue_Object_singleton_const::IsMutable(void) const
{
	return false;
}

EidosValue *EidosValue_Object_singleton_const::MutableCopy(void) const
{
	EidosValue_Object_vector *new_vec = new EidosValue_Object_vector();
	
	new_vec->PushElement(value_);
	
	return new_vec;
}

void EidosValue_Object_singleton_const::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton_const::SetValueAtIndex): internal error: EidosValue_Object_singleton_const is not modifiable." << eidos_terminate();
}

void EidosValue_Object_singleton_const::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton_const::PushValueFromIndexOfEidosValue): internal error: EidosValue_Object_singleton_const is not modifiable." << eidos_terminate();
}

std::vector<std::string> EidosValue_Object_singleton_const::ReadOnlyMembersOfElements(void) const
{
	return value_->ReadOnlyMembers();
}

std::vector<std::string> EidosValue_Object_singleton_const::ReadWriteMembersOfElements(void) const
{
	return value_->ReadWriteMembers();
}

EidosValue *EidosValue_Object_singleton_const::GetValueForMemberOfElements(EidosGlobalStringID p_member_id) const
{
	EidosValue *result = value_->GetValueForMember(p_member_id);
	
	if (result->Count() != 1)
	{
		// We need to check that this property is const; if not, it is required to give a singleton return
		if (!value_->MemberIsReadOnly(p_member_id))
			EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton_const::GetValueForMemberOfElements): internal error: non-const member " << StringForEidosGlobalStringID(p_member_id) << " produced " << result->Count() << " values for a single element." << eidos_terminate();
	}
	
	return result;
}

// This somewhat odd method returns one "representative" EidosValue for the given property, by calling the first element in the
// object.  This is used by code completion to follow the chain of object types along a key path; we don't need all of the values
// that the property would return, we just need one representative value of the proper type.  This is more efficient, of course;
// but the main reason that we don't just call GetValueForMemberOfElements() is that we need an API that will not raise.
EidosValue *EidosValue_Object_singleton_const::GetRepresentativeValueOrNullForMemberOfElements(EidosGlobalStringID p_member_id) const
{
	// check that the member is defined before we call our elements
	const std::string &member_name = StringForEidosGlobalStringID(p_member_id);
	std::vector<std::string> constant_members = value_->ReadOnlyMembers();
	
	if (std::find(constant_members.begin(), constant_members.end(), member_name) == constant_members.end())
	{
		std::vector<std::string> variable_members = value_->ReadWriteMembers();
		
		if (std::find(variable_members.begin(), variable_members.end(), member_name) == variable_members.end())
			return nullptr;
	}
	
	// get a value from the first element and return it; we only need to return one representative value
	return value_->GetValueForMember(p_member_id);
}

void EidosValue_Object_singleton_const::SetValueForMemberOfElements(EidosGlobalStringID p_member_id, EidosValue *p_value)
{
	if (p_value->Count() == 1)
		value_->SetValueForMember(p_member_id, p_value);
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton_const::SetValueForMemberOfElements): assignment to a member requires an rvalue that is a singleton (multiplex assignment) or that has a .size() matching the .size of the lvalue." << eidos_terminate();
}

std::vector<std::string> EidosValue_Object_singleton_const::MethodsOfElements(void) const
{
	return value_->Methods();
}

const EidosMethodSignature *EidosValue_Object_singleton_const::SignatureForMethodOfElements(EidosGlobalStringID p_method_id) const
{
	return value_->SignatureForMethod(p_method_id);
}

EidosValue *EidosValue_Object_singleton_const::ExecuteClassMethodOfElements(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	return value_->ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}

EidosValue *EidosValue_Object_singleton_const::ExecuteInstanceMethodOfElements(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	return value_->ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}


//
//	EidosObjectElement
//
#pragma mark EidosObjectElement

EidosObjectElement::EidosObjectElement(void)
{
}

EidosObjectElement::~EidosObjectElement(void)
{
}

void EidosObjectElement::Print(std::ostream &p_ostream) const
{
	p_ostream << *ElementType();
}

EidosObjectElement *EidosObjectElement::Retain(void)
{
	// no-op; our lifetime is controlled externally
	return this;
}

EidosObjectElement *EidosObjectElement::Release(void)
{
	// no-op; our lifetime is controlled externally
	return this;
}

std::vector<std::string> EidosObjectElement::ReadOnlyMembers(void) const
{
	return std::vector<std::string>();	// no read-only members
}

std::vector<std::string> EidosObjectElement::ReadWriteMembers(void) const
{
	return std::vector<std::string>();	// no read-write members
}

bool EidosObjectElement::MemberIsReadOnly(EidosGlobalStringID p_member_id) const
{
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::MemberIsReadOnly for " << *ElementType() << "): unrecognized member name \"" << StringForEidosGlobalStringID(p_member_id) << "\"." << eidos_terminate();
	return true;
}

EidosValue *EidosObjectElement::GetValueForMember(EidosGlobalStringID p_member_id)
{
	bool readonly = MemberIsReadOnly(p_member_id);	// will raise if the member does not exist at all
	
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::GetValueForMember for " << *ElementType() << "): internal error: attempt to get a value for " << (readonly ? "read-only member " : "read-write member ") << StringForEidosGlobalStringID(p_member_id) << " was not handled by subclass." << eidos_terminate();
	
	return nullptr;
}

void EidosObjectElement::SetValueForMember(EidosGlobalStringID p_member_id, EidosValue *p_value)
{
#pragma unused(p_value)
	bool readonly = MemberIsReadOnly(p_member_id);	// will raise if the member does not exist at all
	
	// Check whether setting a constant was attempted; we can do this on behalf of all our subclasses
	if (readonly)
		EIDOS_TERMINATION << "ERROR (EidosObjectElement::SetValueForMember for " << *ElementType() << "): attempt to set a new value for read-only member " << StringForEidosGlobalStringID(p_member_id) << "." << eidos_terminate();
	else
		EIDOS_TERMINATION << "ERROR (EidosObjectElement::SetValueForMember for " << *ElementType() << "): internal error: setting a new value for read-write member " << StringForEidosGlobalStringID(p_member_id) << " was not handled by subclass." << eidos_terminate();
}

std::vector<std::string> EidosObjectElement::Methods(void) const
{
	std::vector<std::string> methods;
	
	methods.push_back(gStr_method);
	methods.push_back(gStr_property);
	methods.push_back(gStr_str);
	
	return methods;
}

const EidosMethodSignature *EidosObjectElement::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	// Signatures are all preallocated, for speed
	static EidosInstanceMethodSignature *strSig = nullptr;
	static EidosClassMethodSignature *propertySig = nullptr;
	static EidosClassMethodSignature *methodsSig = nullptr;
	
	if (!strSig)
	{
		methodsSig = (EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_method, kValueMaskNULL))->AddString_OS("methodName");
		propertySig = (EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_property, kValueMaskNULL))->AddString_OS("propertyName");
		strSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_str, kValueMaskNULL));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID_method:
			return methodsSig;
		case gID_property:
			return propertySig;
		case gID_str:
			return strSig;
			
			// all others, including gID_none
		default:
			// Check whether the method signature request failed due to a bad subclass implementation
			std::vector<std::string> methods = Methods();
			const std::string &method_name = StringForEidosGlobalStringID(p_method_id);
			
			if (std::find(methods.begin(), methods.end(), method_name) != methods.end())
				EIDOS_TERMINATION << "ERROR (EidosObjectElement::SignatureForMethod for " << *ElementType() << "): internal error: method signature " << &method_name << " was not provided by subclass." << eidos_terminate();
			
			// Otherwise, we have an unrecognized method, so throw
			EIDOS_TERMINATION << "ERROR (EidosObjectElement::SignatureForMethod for " << *ElementType() << "): unrecognized method name " << &method_name << "." << eidos_terminate();
			return new EidosInstanceMethodSignature(gStr_empty_string, kValueMaskNULL);
	}
}

EidosValue *EidosObjectElement::ExecuteMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused(p_arguments, p_interpreter)
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID_str:		// instance method
		{
			std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
			
			output_stream << *ElementType() << ":" << endl;
			
			std::vector<std::string> read_only_member_names = ReadOnlyMembers();
			std::vector<std::string> read_write_member_names = ReadWriteMembers();
			std::vector<std::string> member_names;
			
			member_names.insert(member_names.end(), read_only_member_names.begin(), read_only_member_names.end());
			member_names.insert(member_names.end(), read_write_member_names.begin(), read_write_member_names.end());
			std::sort(member_names.begin(), member_names.end());
			
			for (auto member_name_iter = member_names.begin(); member_name_iter != member_names.end(); ++member_name_iter)
			{
				const std::string &member_name = *member_name_iter;
				EidosGlobalStringID member_id = EidosGlobalStringIDForString(member_name);
				EidosValue *member_value = GetValueForMember(member_id);
				int member_count = member_value->Count();
				bool is_const = std::find(read_only_member_names.begin(), read_only_member_names.end(), member_name) != read_only_member_names.end();
				
				output_stream << "\t";
				
				if (member_count <= 2)
					output_stream << member_name << (is_const ? " => (" : " -> (") << member_value->Type() << ") " << *member_value << endl;
				else
				{
					EidosValue *first_value = member_value->GetValueAtIndex(0);
					EidosValue *second_value = member_value->GetValueAtIndex(1);
					
					output_stream << member_name << (is_const ? " => (" : " -> (") << member_value->Type() << ") " << *first_value << " " << *second_value << " ... (" << member_count << " values)" << endl;
					
					if (first_value->IsTemporary()) delete first_value;
					if (second_value->IsTemporary()) delete second_value;
				}
				
				if (member_value->IsTemporary()) delete member_value;
			}
			
			return gStaticEidosValueNULLInvisible;
		}
		case gID_property:		// class method
		{
			std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
			bool has_match_string = (p_argument_count == 1);
			string match_string = (has_match_string ? p_arguments[0]->StringAtIndex(0) : gStr_empty_string);
			std::vector<std::string> read_only_member_names = ReadOnlyMembers();
			std::vector<std::string> read_write_member_names = ReadWriteMembers();
			std::vector<std::string> member_names;
			bool signature_found = false;
			
			member_names.insert(member_names.end(), read_only_member_names.begin(), read_only_member_names.end());
			member_names.insert(member_names.end(), read_write_member_names.begin(), read_write_member_names.end());
			std::sort(member_names.begin(), member_names.end());
			
			for (auto member_name_iter = member_names.begin(); member_name_iter != member_names.end(); ++member_name_iter)
			{
				const std::string &member_name = *member_name_iter;
				EidosGlobalStringID member_id = EidosGlobalStringIDForString(member_name);
				
				if (has_match_string && (member_name.compare(match_string) != 0))
					continue;
				
				EidosValue *member_value = GetValueForMember(member_id);
				bool is_const = std::find(read_only_member_names.begin(), read_only_member_names.end(), member_name) != read_only_member_names.end();
				
				output_stream << member_name << (is_const ? " => (" : " -> (") << member_value->Type() << ")" << endl;
				
				if (member_value->IsTemporary()) delete member_value;
				signature_found = true;
			}
			
			if (has_match_string && !signature_found)
				output_stream << "No property found for \"" << match_string << "\"." << endl;
			
			return gStaticEidosValueNULLInvisible;
		}
		case gID_method:		// class method
		{
			std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
			bool has_match_string = (p_argument_count == 1);
			string match_string = (has_match_string ? p_arguments[0]->StringAtIndex(0) : gStr_empty_string);
			std::vector<std::string> method_names = Methods();
			bool signature_found = false;
			
			std::sort(method_names.begin(), method_names.end());
			
			for (auto method_name_iter = method_names.begin(); method_name_iter != method_names.end(); ++method_name_iter)
			{
				const std::string &method_name = *method_name_iter;
				EidosGlobalStringID method_id = EidosGlobalStringIDForString(method_name);
				
				if (has_match_string && (method_name.compare(match_string) != 0))
					continue;
				
				const EidosMethodSignature *method_signature = SignatureForMethod(method_id);
				
				output_stream << *method_signature << endl;
				signature_found = true;
			}
			
			if (has_match_string && !signature_found)
				output_stream << "No method signature found for \"" << match_string << "\"." << endl;
			
			return gStaticEidosValueNULLInvisible;
		}
			
			// all others, including gID_none
		default:
		{
			// Check whether the method call failed due to a bad subclass implementation
			std::vector<std::string> methods = Methods();
			const std::string &method_name = StringForEidosGlobalStringID(p_method_id);
			
			if (std::find(methods.begin(), methods.end(), method_name) != methods.end())
				EIDOS_TERMINATION << "ERROR (EidosObjectElement::ExecuteMethod for " << *ElementType() << "): internal error: method " << method_name << " was not handled by subclass." << eidos_terminate();
			
			// Otherwise, we have an unrecognized method, so throw
			EIDOS_TERMINATION << "ERROR (EidosObjectElement::ExecuteMethod for " << *ElementType() << "): unrecognized method name " << method_name << "." << eidos_terminate();
			
			return gStaticEidosValueNULLInvisible;
		}
	}
}

void EidosObjectElement::TypeCheckValue(const std::string &p_method_name, EidosGlobalStringID p_member_id, EidosValue *p_value, EidosValueMask p_type_mask)
{
	uint32_t typemask = p_type_mask;
	bool type_ok = true;
	
	switch (p_value->Type())
	{
		case EidosValueType::kValueNULL:		type_ok = !!(typemask & kValueMaskNULL);		break;
		case EidosValueType::kValueLogical:	type_ok = !!(typemask & kValueMaskLogical);	break;
		case EidosValueType::kValueInt:		type_ok = !!(typemask & kValueMaskInt);		break;
		case EidosValueType::kValueFloat:		type_ok = !!(typemask & kValueMaskFloat);		break;
		case EidosValueType::kValueString:		type_ok = !!(typemask & kValueMaskString);	break;
		case EidosValueType::kValueObject:		type_ok = !!(typemask & kValueMaskObject);	break;
	}
	
	if (!type_ok)
		EIDOS_TERMINATION << "ERROR (EidosObjectElement::TypeCheckValue for " << *ElementType() << "::" << p_method_name << "): type " << p_value->Type() << " is not legal for member " << StringForEidosGlobalStringID(p_member_id) << "." << eidos_terminate();
}

void EidosObjectElement::RangeCheckValue(const std::string &p_method_name, EidosGlobalStringID p_member_id, bool p_in_range)
{
	if (!p_in_range)
		EIDOS_TERMINATION << "ERROR (EidosObjectElement::RangeCheckValue for" << *ElementType() << "::" << p_method_name << "): new value for member " << StringForEidosGlobalStringID(p_member_id) << " is illegal." << eidos_terminate();
}

std::ostream &operator<<(std::ostream &p_outstream, const EidosObjectElement &p_element)
{
	p_element.Print(p_outstream);	// get dynamic dispatch
	
	return p_outstream;
}


//
//	EidosObjectElementInternal
//
#pragma mark EidosObjectElementInternal

EidosObjectElementInternal::EidosObjectElementInternal(void)
{
//	std::cerr << "EidosObjectElementInternal::EidosObjectElementInternal allocated " << this << " with refcount == 1" << endl;
//	eidos_print_stacktrace(stderr, 10);
}

EidosObjectElementInternal::~EidosObjectElementInternal(void)
{
//	std::cerr << "EidosObjectElementInternal::~EidosObjectElementInternal deallocated " << this << endl;
//	eidos_print_stacktrace(stderr, 10);
}

EidosObjectElement *EidosObjectElementInternal::Retain(void)
{
	refcount_++;
	
//	std::cerr << "EidosObjectElementInternal::Retain for " << this << ", new refcount == " << refcount_ << endl;
//	eidos_print_stacktrace(stderr, 10);
	
	return this;
}

EidosObjectElement *EidosObjectElementInternal::Release(void)
{
	refcount_--;
	
//	std::cerr << "EidosObjectElementInternal::Release for " << this << ", new refcount == " << refcount_ << endl;
//	eidos_print_stacktrace(stderr, 10);
	
	if (refcount_ == 0)
	{
		delete this;
		return nullptr;
	}
	
	return this;
}






































































