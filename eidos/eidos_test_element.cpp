//
//  eidos_test_element.cpp
//  Eidos
//
//  Created by Ben Haller on 5/1/15.
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


#include "eidos_test_element.h"
#include "eidos_functions.h"
#include "eidos_call_signature.h"
#include "eidos_global.h"

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>


using std::string;
using std::vector;
using std::endl;


//
//	Eidos_TestElement
//
#pragma mark Eidos_TestElement

Eidos_TestElement::Eidos_TestElement(int64_t p_value) : yolk_(p_value)
{
}

const std::string *Eidos_TestElement::ElementType(void) const
{
	return &gStr__TestElement;
}

std::vector<std::string> Eidos_TestElement::ReadOnlyMembers(void) const
{
	std::vector<std::string> members;
	
	return members;
}

std::vector<std::string> Eidos_TestElement::ReadWriteMembers(void) const
{
	std::vector<std::string> members;
	
	members.push_back(gStr__yolk);
	
	return members;
}

bool Eidos_TestElement::MemberIsReadOnly(EidosGlobalStringID p_member_id) const
{
	if (p_member_id == gID__yolk)
		return false;
	else
		return EidosObjectElement::MemberIsReadOnly(p_member_id);
}

EidosValue *Eidos_TestElement::GetValueForMember(EidosGlobalStringID p_member_id)
{
	if (p_member_id == gID__yolk)
		return new EidosValue_Int_singleton_const(yolk_);
	
	// all others, including gID_none
	else
		return EidosObjectElement::GetValueForMember(p_member_id);
}

void Eidos_TestElement::SetValueForMember(EidosGlobalStringID p_member_id, EidosValue *p_value)
{
	if (p_member_id == gID__yolk)
	{
		EidosValueType value_type = p_value->Type();
		int value_count = p_value->Count();
		
		if (value_type != EidosValueType::kValueInt)
			EIDOS_TERMINATION << "ERROR (Eidos_TestElement::SetValueForMember): type mismatch in assignment to member 'path'." << eidos_terminate();
		if (value_count != 1)
			EIDOS_TERMINATION << "ERROR (Eidos_TestElement::SetValueForMember): value of size() == 1 expected in assignment to member 'path'." << eidos_terminate();
		
		yolk_ = p_value->IntAtIndex(0);
		return;
	}
	
	// all others, including gID_none
	else
		return EidosObjectElement::SetValueForMember(p_member_id, p_value);
}

std::vector<std::string> Eidos_TestElement::Methods(void) const
{
	std::vector<std::string> methods = EidosObjectElement::Methods();
	
	methods.push_back(gStr__cubicYolk);
	
	return methods;
}

const EidosMethodSignature *Eidos_TestElement::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	// Signatures are all preallocated, for speed
	static EidosInstanceMethodSignature *cubicYolkSig = nullptr;
	
	if (!cubicYolkSig)
	{
		cubicYolkSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr__cubicYolk, kValueMaskInt | kValueMaskSingleton));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID__cubicYolk:
			return cubicYolkSig;
			
			// all others, including gID_none
		default:
			return EidosObjectElement::SignatureForMethod(p_method_id);
	}
}

EidosValue *Eidos_TestElement::ExecuteMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID__cubicYolk:
		{
			return new EidosValue_Int_singleton_const(yolk_ * yolk_ * yolk_);
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}



































































