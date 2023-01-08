#include "DifferentialHybridImage.h"

#include "vm/Image.h"
#include "vm/MetadataAlloc.h"
#include "metadata/GenericMethod.h"

#include "MetadataModule.h"

#if HYBRIDCLR_ENABLE_DHE

namespace hybridclr
{
namespace metadata
{

	const Il2CppGenericInst* DifferentialHybridImage::TranslateGenericInstToDHE(const Il2CppGenericInst* gi)
	{
		if (!gi)
		{
			return gi;
		}
		const Il2CppType* tempTypeArgv[32];
		IL2CPP_ASSERT(gi->type_argc <= 32);
		bool change = false;
		for (uint32_t i = 0; i < gi->type_argc; i++)
		{
			const Il2CppType* argType = gi->type_argv[i];
			const Il2CppType* dhArgType = TranslateIl2CppTypeToDHE(argType);
			change = change || (argType != dhArgType);
			tempTypeArgv[i] = dhArgType;
		}
		if (!change)
		{
			return gi;
		}
		Il2CppGenericInst* dhGi = MallocMetadataWithLock<Il2CppGenericInst>();
		Il2CppType** newArgTypes = CallocMetadataWithLock<Il2CppType*>(gi->type_argc);
		std::memcpy(newArgTypes, tempTypeArgv, sizeof(Il2CppType*) * gi->type_argc);
		dhGi->type_argc = gi->type_argc;
		dhGi->type_argv = (const Il2CppType**)newArgTypes;
		return dhGi;
	}

	const Il2CppType* DifferentialHybridImage::TranslateIl2CppTypeToDHE(const Il2CppType* type)
	{
		switch (type->type)
		{
		case IL2CPP_TYPE_OBJECT:
		case IL2CPP_TYPE_VOID:
		case IL2CPP_TYPE_BOOLEAN:
		case IL2CPP_TYPE_CHAR:
		case IL2CPP_TYPE_I1:
		case IL2CPP_TYPE_U1:
		case IL2CPP_TYPE_I2:
		case IL2CPP_TYPE_U2:
		case IL2CPP_TYPE_I4:
		case IL2CPP_TYPE_U4:
		case IL2CPP_TYPE_I:
		case IL2CPP_TYPE_U:
		case IL2CPP_TYPE_I8:
		case IL2CPP_TYPE_U8:
		case IL2CPP_TYPE_R4:
		case IL2CPP_TYPE_R8:
		case IL2CPP_TYPE_STRING:
		case IL2CPP_TYPE_TYPEDBYREF:
			return type;
		case IL2CPP_TYPE_ARRAY:
		{
			const Il2CppType* eleType = type->data.array->etype;
			const Il2CppType* dheEleType = TranslateIl2CppTypeToDHE(eleType);
			if (eleType == dheEleType)
			{
				return type;
			}
			Il2CppType* newType = MallocMetadataWithLock<Il2CppType>();
			*newType = *type;
			newType->data.array->etype = dheEleType;
			return newType;
		}
		case IL2CPP_TYPE_FNPTR:
			IL2CPP_NOT_IMPLEMENTED(Class::FromIl2CppType);
			return NULL; //mono_fnptr_class_get (type->data.method);
		case IL2CPP_TYPE_PTR:
		case IL2CPP_TYPE_SZARRAY:
		{
			const Il2CppType* eleType = type->data.type;
			const Il2CppType* dheEleType = TranslateIl2CppTypeToDHE(eleType);
			if (eleType == dheEleType)
			{
				return type;
			}
			Il2CppType* newType = MallocMetadataWithLock<Il2CppType>();
			*newType = *type;
			newType->data.type = dheEleType;
			return newType;
		}
		case IL2CPP_TYPE_CLASS:
		case IL2CPP_TYPE_VALUETYPE:
		{
			Il2CppTypeDefinition* typeDef = (Il2CppTypeDefinition*)type->data.typeHandle;
			if (hybridclr::metadata::IsInterpreterType(typeDef))
			{
				return type;
			}
			TypeIndex typeIndex = il2cpp::vm::GlobalMetadata::GetIndexForTypeDefinition(typeDef);
			const Il2CppImage* image = il2cpp::vm::GlobalMetadata::GetImageForTypeDefinitionIndex(typeIndex);
			if (!IsAOTImageDifferentialHbyridImage(image))
			{
				return type;
			}
			const Il2CppImage* dhImage = image->assembly->dheAssembly->image;
			const Il2CppTypeDefinition* dhTypeDef = ((hybridclr::metadata::DifferentialHybridImage*)hybridclr::metadata::MetadataModule::GetImage(dhImage))->GetInterpreterTypeByOriginType(typeDef);
			if (!dhTypeDef)
			{
				return type;
			}
			Il2CppType* newType = MallocMetadataWithLock<Il2CppType>();
			*newType = *type;
			newType->data.typeHandle = (Il2CppMetadataTypeHandle)dhTypeDef;
			return newType;
		}
		case IL2CPP_TYPE_GENERICINST:
		{
			Il2CppGenericClass* gc = type->data.generic_class;
			const Il2CppType* dhType = TranslateIl2CppTypeToDHE(gc->type);
			const Il2CppGenericInst* klassGi = TranslateGenericInstToDHE(gc->context.class_inst);
			const Il2CppGenericInst* methodGi = TranslateGenericInstToDHE(gc->context.method_inst);
			if (dhType == gc->type && klassGi == gc->context.class_inst && methodGi == gc->context.method_inst)
			{
				return type;
			}
			Il2CppGenericClass* dhGc = MallocMetadataWithLock<Il2CppGenericClass>();
			dhGc->type = dhType;
			dhGc->context = { klassGi, methodGi };
			dhGc->cached_class = nullptr;
			Il2CppType* newType = MallocMetadataWithLock<Il2CppType>();
			*newType = *type;
			newType->data.generic_class = dhGc;
			return newType;
		}
		case IL2CPP_TYPE_VAR:
		case IL2CPP_TYPE_MVAR:
			il2cpp::vm::Exception::Raise(il2cpp::vm::Exception::GetExecutionEngineException("TranslateIl2CppTypeFromAOTToDHE not support generic var"));
		default:
			IL2CPP_NOT_IMPLEMENTED(Class::FromIl2CppType);
		}
		return nullptr;
	}

	const Il2CppMethodDefinition* DifferentialHybridImage::TranslateMethodDefinitionToDHE(const Il2CppMethodDefinition* methodDef)
	{
		Il2CppTypeDefinition* typeDef = (Il2CppTypeDefinition*)il2cpp::vm::GlobalMetadata::GetTypeHandleFromIndex(methodDef->declaringType);

		TypeIndex typeIndex = il2cpp::vm::GlobalMetadata::GetIndexForTypeDefinition(typeDef);
		const Il2CppImage* image = il2cpp::vm::GlobalMetadata::GetImageForTypeDefinitionIndex(typeIndex);
		if (IsAOTImageDifferentialHbyridImage(image))
		{
			const Il2CppImage* dhImage = image->assembly->dheAssembly->image;
			const Il2CppMethodDefinition* dhMethodDef = ((hybridclr::metadata::DifferentialHybridImage*)hybridclr::metadata::MetadataModule::GetImage(dhImage))
				->GetInterpreterMethodDefinitionByOriginMethodDefinition(methodDef);
			if (dhMethodDef)
			{
				return dhMethodDef;
			}
		}
		return methodDef;
	}


	const Il2CppGenericMethod* DifferentialHybridImage::TranslateGenericMethodToDHE(const Il2CppGenericMethod* genericMethod)
	{
		// 由于genericmethod的初始化时机早，全部指向最早的image指针，没有指向被处理后的image->assembly->dhassembly->image
		const Il2CppImage* image = genericMethod->methodDefinition->klass->image;

		const Il2CppGenericContext& gc = genericMethod->context;
		const Il2CppGenericInst* klassGi = TranslateGenericInstToDHE(gc.class_inst);
		const Il2CppGenericInst* methodGi = TranslateGenericInstToDHE(gc.method_inst);
		const MethodInfo* dhMethod = genericMethod->methodDefinition;

		if (hybridclr::metadata::IsDifferentialHybridImage(image))
		{
			const Il2CppMethodDefinition* dhMethodDef = TranslateMethodDefinitionToDHE((Il2CppMethodDefinition*)dhMethod->methodMetadataHandle);
			if ((Il2CppMetadataMethodDefinitionHandle)dhMethodDef != dhMethod->methodMetadataHandle)
			{
				dhMethod = il2cpp::vm::GlobalMetadata::GetMethodInfoFromMethodHandle((Il2CppMetadataMethodDefinitionHandle)dhMethodDef);
			}
		}

		const Il2CppGenericMethod* dhGenericMethod;
		if (dhMethod == genericMethod->methodDefinition && klassGi == gc.class_inst && methodGi == gc.method_inst)
		{
			dhGenericMethod = genericMethod;
		}
		else
		{
			Il2CppGenericMethod* newGenericMethod = il2cpp::vm::MetadataAllocGenericMethod();
			*newGenericMethod = { dhMethod, {klassGi, methodGi} };
			dhGenericMethod = newGenericMethod;
		}
		return dhGenericMethod;
	}

	const Il2CppType* DifferentialHybridImage::TranslateReverseGenericShareIl2CppTypeFromDHE(const Il2CppType* type)
	{
		switch (type->type)
		{
		case IL2CPP_TYPE_OBJECT:
		case IL2CPP_TYPE_VOID:
		case IL2CPP_TYPE_BOOLEAN:
		case IL2CPP_TYPE_CHAR:
		case IL2CPP_TYPE_I1:
		case IL2CPP_TYPE_U1:
		case IL2CPP_TYPE_I2:
		case IL2CPP_TYPE_U2:
		case IL2CPP_TYPE_I4:
		case IL2CPP_TYPE_U4:
		case IL2CPP_TYPE_I:
		case IL2CPP_TYPE_U:
		case IL2CPP_TYPE_I8:
		case IL2CPP_TYPE_U8:
		case IL2CPP_TYPE_R4:
		case IL2CPP_TYPE_R8:
		case IL2CPP_TYPE_STRING:
		case IL2CPP_TYPE_TYPEDBYREF:
		case IL2CPP_TYPE_ARRAY:
			return type;
		case IL2CPP_TYPE_FNPTR:
			IL2CPP_NOT_IMPLEMENTED(Class::FromIl2CppType);
			return NULL; //mono_fnptr_class_get (type->data.method);
		case IL2CPP_TYPE_PTR:
		case IL2CPP_TYPE_SZARRAY:
		case IL2CPP_TYPE_CLASS:
			return type;
		case IL2CPP_TYPE_VALUETYPE:
		{
			Il2CppTypeDefinition* typeDef = (Il2CppTypeDefinition*)type->data.typeHandle;
			if (!hybridclr::metadata::IsInterpreterType(typeDef))
			{
				return type;
			}
			InterpreterImage* image = MetadataModule::GetImage(typeDef);
			if (!IsDifferentialHybridImage(image))
			{
				return type;
			}
			DifferentialHybridImage* dhImage = (DifferentialHybridImage*)image;
			const Il2CppTypeDefinition* aotTypeDef = dhImage->GetGenericShareOriginType(typeDef);
			if (!aotTypeDef)
			{
				return type;
			}
			return il2cpp::vm::GlobalMetadata::GetIl2CppTypeFromIndex(aotTypeDef->byvalTypeIndex);
		}
		case IL2CPP_TYPE_GENERICINST:
		{
			Il2CppGenericClass* gc = type->data.generic_class;
			if (!IsValueType((Il2CppTypeDefinition*)gc->type->data.typeHandle))
			{
				return type;
			}
			const Il2CppType* dhType = TranslateReverseGenericShareIl2CppTypeFromDHE(gc->type);
			const Il2CppGenericInst* klassGi = TranslateReverseGenericShareGenericInstFromDHE(gc->context.class_inst);
			const Il2CppGenericInst* methodGi = TranslateReverseGenericShareGenericInstFromDHE(gc->context.method_inst);
			if (dhType == gc->type && klassGi == gc->context.class_inst && methodGi == gc->context.method_inst)
			{
				return type;
			}
			Il2CppGenericClass* dhGc = MallocMetadataWithLock<Il2CppGenericClass>();
			dhGc->type = dhType;
			dhGc->context = { klassGi, methodGi };
			dhGc->cached_class = nullptr;
			Il2CppType* newType = MallocMetadataWithLock<Il2CppType>();
			*newType = *type;
			newType->data.generic_class = dhGc;
			return newType;
		}
		case IL2CPP_TYPE_VAR:
		case IL2CPP_TYPE_MVAR:
			il2cpp::vm::Exception::Raise(il2cpp::vm::Exception::GetExecutionEngineException("TranslateIl2CppTypeFromAOTToDHE not support generic var"));
		default:
			IL2CPP_NOT_IMPLEMENTED(Class::FromIl2CppType);
		}
		return nullptr;
	}

	const Il2CppGenericInst* DifferentialHybridImage::TranslateReverseGenericShareGenericInstFromDHE(const Il2CppGenericInst* gi)
	{
		if (!gi)
		{
			return gi;
		}
		const Il2CppType* tempTypeArgv[32];
		IL2CPP_ASSERT(gi->type_argc <= 32);
		bool change = false;
		for (uint32_t i = 0; i < gi->type_argc; i++)
		{
			const Il2CppType* argType = gi->type_argv[i];
			const Il2CppType* dhArgType = TranslateReverseGenericShareIl2CppTypeFromDHE(argType);
			change = change || (argType != dhArgType);
			tempTypeArgv[i] = dhArgType;
		}
		if (!change)
		{
			return gi;
		}
		Il2CppGenericInst* dhGi = MallocMetadataWithLock<Il2CppGenericInst>();
		Il2CppType** newArgTypes = CallocMetadataWithLock<Il2CppType*>(gi->type_argc);
		std::memcpy(newArgTypes, tempTypeArgv, sizeof(Il2CppType*) * gi->type_argc);
		dhGi->type_argc = gi->type_argc;
		dhGi->type_argv = (const Il2CppType**)newArgTypes;
		return dhGi;
	}

	void DifferentialHybridImage::BuildIl2CppImage(Il2CppImage* image, Il2CppAssembly* originAssembly)
	{
		InterpreterImage::BuildIl2CppImage(image);
		image->assembly->originAssembly = originAssembly;
		_originAssembly = originAssembly;
		_originImage = originAssembly->image;
	}

	void DifferentialHybridImage::InitTypeMapping(TypeMapping& type)
	{
		if (type.inited)
		{
			return;
		}
		type.inited = true;
		if (type.interpType->declaringTypeIndex != kTypeDefinitionIndexInvalid)
		{
			const Il2CppTypeDefinition* declaringTypeDef = (const Il2CppTypeDefinition*)GetIl2CppTypeFromRawIndex(DecodeMetadataIndex(type.interpType->declaringTypeIndex))->data.typeHandle;
			uint32_t declaringTypeIndex = GetTypeRawIndex(declaringTypeDef);
			TypeMapping& declaringTm = _typeMappings[declaringTypeIndex];
			InitTypeMapping(declaringTm);
			if (declaringTm.aotType == nullptr)
			{
				return;
			}
			void* iter = nullptr;
			for (const Il2CppTypeDefinition* nextTypeDef; (nextTypeDef = (const Il2CppTypeDefinition*)il2cpp::vm::GlobalMetadata::GetNestedTypes((Il2CppMetadataTypeHandle)declaringTm.aotType, &iter));)
			{
				const char* nestedTypeName = il2cpp::vm::GlobalMetadata::GetStringFromIndex(nextTypeDef->nameIndex);
				IL2CPP_ASSERT(nestedTypeName);
				if (!std::strcmp(type.name, nestedTypeName))
				{
					type.aotType = nextTypeDef;
					break;
				}
			}
		}
		else
		{
			const char* namespaze = il2cpp::vm::GlobalMetadata::GetStringFromIndex(type.interpType->namespaceIndex);
			const Il2CppTypeDefinition* typeDef = (const Il2CppTypeDefinition*)il2cpp::vm::Image::TypeHandleFromName(_originImage, namespaze, type.name);
			type.aotType = typeDef;
		}
	}

	void DifferentialHybridImage::InitTypeMappings()
	{
		_typeMappings.resize(_typeDetails.size());
		for (TypeDefinitionDetail& td : _typeDetails)
		{
			TypeMapping& tm = _typeMappings[td.index];
			tm.inited = false;
			tm.name = _rawImage.GetStringFromRawIndex(DecodeMetadataIndex(td.typeDef->nameIndex));
			tm.interpType = td.typeDef;
			tm.aotType = nullptr;
		}
		for (TypeMapping& type : _typeMappings)
		{
			InitTypeMapping(type);
			if (type.aotType)
			{
				_originType2InterpreterTypes[type.aotType] = type.interpType;
				_interpreterToken2OriginAOTTokens[type.interpType->token] = type.aotType->token;
				type.notChanged = _unchangedClassTokens.find(type.interpType->token) != _unchangedClassTokens.end();
			}
		}
	}

	Il2CppMetadataTypeHandle GetOriginAOTTypeHandle(Il2CppMetadataTypeHandle typeHandle)
	{
		const Il2CppTypeDefinition* typeDef = (const Il2CppTypeDefinition*)typeHandle;
		if (IsInterpreterType(typeDef))
		{
			InterpreterImage* image = MetadataModule::GetImage(typeDef);
			if (IsDifferentialHybridImage(image))
			{
				const Il2CppTypeDefinition* originTypeDef = ((DifferentialHybridImage*)image)->GetOriginType(typeDef);
				if (originTypeDef)
				{
					return (Il2CppMetadataTypeHandle)originTypeDef;
				}
			}
		}
		return typeHandle;
	}

	bool IsDHETypeEqual(const Il2CppType* interpType, const Il2CppType* aotType)
	{
		if (aotType->type != interpType->type)
		{
			return false;
		}

		if (aotType->byref != interpType->byref)
		{
			return false;
		}

		switch (aotType->type)
		{
		case IL2CPP_TYPE_VALUETYPE:
		case IL2CPP_TYPE_CLASS:
			return aotType->data.typeHandle == GetOriginAOTTypeHandle(interpType->data.typeHandle);
		case IL2CPP_TYPE_PTR:
		case IL2CPP_TYPE_SZARRAY:
			return IsDHETypeEqual(aotType->data.type, interpType->data.type);

		case IL2CPP_TYPE_ARRAY:
		{
			if (aotType->data.array->rank < interpType->data.array->rank)
			{
				return false;
			}
			return IsDHETypeEqual(aotType->data.array->etype, interpType->data.array->etype);
		}
		case IL2CPP_TYPE_GENERICINST:
		{
			const Il2CppGenericInst* i1 = aotType->data.generic_class->context.class_inst;
			const Il2CppGenericInst* i2 = interpType->data.generic_class->context.class_inst;

			// this happens when maximum generic recursion is hit
			if (i1 == NULL || i2 == NULL)
			{
				return i1 == i2;
			}

			if (i1->type_argc != i2->type_argc)
				return false;

			if (!IsDHETypeEqual(aotType->data.generic_class->type, interpType->data.generic_class->type))
				return false;

			/* FIXME: we should probably just compare the instance pointers directly.  */
			for (uint32_t i = 0; i < i1->type_argc; ++i)
			{
				if (!IsDHETypeEqual(i1->type_argv[i], i2->type_argv[i]))
				{
					return false;
				}
			}

			return true;
		}
		case IL2CPP_TYPE_VAR:
		case IL2CPP_TYPE_MVAR:
		{
			const Il2CppGenericParameter* interpGenericParam = (const Il2CppGenericParameter*)interpType->data.genericParameterHandle;
			const Il2CppGenericParameter* aotGenericParam = (const Il2CppGenericParameter*)aotType->data.genericParameterHandle;
			return interpGenericParam->num == aotGenericParam->num;
		}
		default:
			return true;
		}
		return false;
	}

	bool IsDHEMethodSigEqual(const Il2CppMethodDefinition* interpMethod, const Il2CppMethodDefinition* aotMethod)
	{
		if (interpMethod->parameterCount != aotMethod->parameterCount)
		{
			return false;
		}
		if (interpMethod->genericContainerIndex == kGenericContainerIndexInvalid)
		{
			if (aotMethod->genericContainerIndex != kGenericContainerIndexInvalid)
			{
				return false;
			}
		}
		else
		{
			if (aotMethod->genericContainerIndex == kGenericContainerIndexInvalid)
			{
				return false;
			}
			const Il2CppGenericContainer* interpMethodGenericContainer = (Il2CppGenericContainer*)il2cpp::vm::GlobalMetadata::GetGenericContainerFromIndex(interpMethod->genericContainerIndex);
			const Il2CppGenericContainer* aotMethodGenericContainer = (Il2CppGenericContainer*)il2cpp::vm::GlobalMetadata::GetGenericContainerFromIndex(aotMethod->genericContainerIndex);
			if (interpMethodGenericContainer->type_argc != aotMethodGenericContainer->type_argc)
			{
				return false;
			}
		}

		const Il2CppType* interpReturnType = il2cpp::vm::GlobalMetadata::GetIl2CppTypeFromIndex(interpMethod->returnType);
		const Il2CppType* aotReturnType = il2cpp::vm::GlobalMetadata::GetIl2CppTypeFromIndex(aotMethod->returnType);
		if (!IsDHETypeEqual(interpReturnType, aotReturnType))
		{
			return false;
		}
		for (uint32_t i = 0; i < interpMethod->parameterCount; i++)
		{
			const Il2CppParameterDefinition* interpParam = (const Il2CppParameterDefinition*)il2cpp::vm::GlobalMetadata::GetParameterDefinitionFromIndex(interpMethod, interpMethod->parameterStart + i);
			const Il2CppParameterDefinition* aotParam = (const Il2CppParameterDefinition*)il2cpp::vm::GlobalMetadata::GetParameterDefinitionFromIndex(aotMethod, aotMethod->parameterStart + i);
			const Il2CppType* interpParamType = il2cpp::vm::GlobalMetadata::GetIl2CppTypeFromIndex(interpParam->typeIndex);
			const Il2CppType* aotParamType = il2cpp::vm::GlobalMetadata::GetIl2CppTypeFromIndex(aotParam->typeIndex);
			if (!IsDHETypeEqual(interpParamType, aotParamType))
			{
				return false;
			}
		}
		return true;
	}

	const Il2CppMethodDefinition* FindMatchMethod(const Il2CppTypeDefinition* aotType, const Il2CppMethodDefinition* toMatchMethod)
	{
		//const Il2CppGenericContainer* klassGenContainer = aotType ? GetGenericContainerFromIl2CppType(&klass1->byval_arg) : nullptr;
		const char* toMatchMethodName = il2cpp::vm::GlobalMetadata::GetStringFromIndex(toMatchMethod->nameIndex);
		for (uint16_t i = 0; i < aotType->method_count; i++)
		{
			const Il2CppMethodDefinition* method1 = il2cpp::vm::GlobalMetadata::GetMethodDefinitionFromIndex(aotType->methodStart + i);
			const char* methodName1 = il2cpp::vm::GlobalMetadata::GetStringFromIndex(method1->nameIndex);
			if (std::strcmp(toMatchMethodName, methodName1))
			{
				continue;
			}
			if (IsDHEMethodSigEqual(method1, toMatchMethod))
			{
				return method1;
			}
		}
		return nullptr;
	}

	FieldIndex FindMatchFieldIndex(const Il2CppTypeDefinition* aotType, const FieldDetail& toMatchField)
	{
		//const Il2CppGenericContainer* klassGenContainer = aotType ? GetGenericContainerFromIl2CppType(&klass1->byval_arg) : nullptr;
		const char* interpFieldName = il2cpp::vm::GlobalMetadata::GetStringFromIndex(toMatchField.fieldDef.nameIndex);
		for (uint16_t i = 0; i < aotType->field_count; i++)
		{
			const Il2CppFieldDefinition* aotField = il2cpp::vm::GlobalMetadata::GetFieldDefinitionFromTypeDefAndFieldIndex(aotType, i);
			const char* interpFieldName = il2cpp::vm::GlobalMetadata::GetStringFromIndex(aotField->nameIndex);
			if (std::strcmp(interpFieldName, interpFieldName))
			{
				continue;
			}
			const Il2CppType* aotFieldType = il2cpp::vm::GlobalMetadata::GetIl2CppTypeFromIndex(aotField->typeIndex);
			const Il2CppType* interpFieldType = il2cpp::vm::GlobalMetadata::GetIl2CppTypeFromIndex(toMatchField.fieldDef.typeIndex);
			if (IsDHETypeEqual(interpFieldType, aotFieldType))
			{
				return (FieldIndex)i;
			}
		}
		return kFieldIndexInvalid;
	}

	void DifferentialHybridImage::InitMethodMappings()
	{
		_methodMappings.resize(_methodDefines.size());

		for (TypeDefinitionDetail& td : _typeDetails)
		{
			TypeMapping& tm = _typeMappings[td.index];
			for (int32_t i = DecodeMetadataIndex(tm.interpType->methodStart), n = i + tm.interpType->method_count; i < n; i++)
			{
				Il2CppMethodDefinition* methodDef = &_methodDefines[i];
				MethodMapping& mm = _methodMappings[i];
				mm.interpMethod = methodDef;

				if (tm.aotType)
				{
					const Il2CppMethodDefinition* originMethod = FindMatchMethod(tm.aotType, methodDef);
					if (originMethod)
					{
						if (_changeMethodTokens.find(methodDef->token) == _changeMethodTokens.end())
						{
							mm.aotMethod = originMethod;
							mm.aotMethodPointer = il2cpp::vm::MetadataCache::GetMethodPointer(_originImage, originMethod->token);
							mm.aotAdjustorThunk = IsValueType(tm.aotType) ? il2cpp::vm::MetadataCache::GetAdjustorThunk(_originImage, mm.aotMethod->token) : nullptr;
							mm.aotMethodInvoker = il2cpp::vm::MetadataCache::GetMethodInvoker(_originImage, originMethod->token);
						}
						_originMethodToken2InterpreterMethodDefs[originMethod->token] = methodDef;
						_interpreterToken2OriginAOTTokens[methodDef->token] = originMethod->token;
					}
				}
			}
		}
	}

	void DifferentialHybridImage::InitFieldMappings()
	{
		for (TypeDefinitionDetail& td : _typeDetails)
		{
			TypeMapping& tm = _typeMappings[td.index];
			if (!tm.aotType)
			{
				continue;
			}
			tm.aotFieldIndex2InterpIndex.assign(tm.aotType->field_count, kFieldIndexInvalid);
			int32_t fieldStartIndex = DecodeMetadataIndex(tm.interpType->fieldStart);
			for (int32_t i = fieldStartIndex, n = fieldStartIndex + tm.interpType->field_count; i < n ; i++)
			{
				FieldDetail& fieldDef = _fieldDetails[i];
				
				if (tm.aotType)
				{
					FieldIndex aotFieldIndex = FindMatchFieldIndex(tm.aotType, fieldDef);
					if (aotFieldIndex != kFieldIndexInvalid)
					{
						tm.aotFieldIndex2InterpIndex[aotFieldIndex] = i - fieldStartIndex;
					}
				}
			}
		}
	}

	void DifferentialHybridImage::InitRuntimeMetadatas()
	{
		InterpreterImage::InitRuntimeMetadatas();
		InitTypeMappings();
		InitMethodMappings();
		InitFieldMappings();
	}

	Il2CppMethodPointer DifferentialHybridImage::TryGetMethodPointer(const Il2CppMethodDefinition* method)
	{
		int32_t index = GetMethodDefinitionRawIndex(method);
		return _methodMappings[index].aotMethodPointer;
	}

	Il2CppMethodPointer DifferentialHybridImage::TryGetMethodPointer(const MethodInfo* method)
	{
		if (method->is_generic)
		{
			return TryGetMethodPointer((const Il2CppMethodDefinition*)method->methodMetadataHandle);
		}

		int32_t index = GetMethodDefinitionRawIndex((Il2CppMethodDefinition*)method->genericMethod->methodDefinition->methodMetadataHandle);
		MethodMapping& mm = _methodMappings[index];
		if (!mm.aotMethod)
		{
			return nullptr;
		}
		const MethodInfo* aotMethod = il2cpp::vm::GlobalMetadata::GetMethodInfoFromMethodHandle((Il2CppMetadataMethodDefinitionHandle)mm.aotMethod);
		const Il2CppGenericContext& originCtx = method->genericMethod->context;
		Il2CppGenericContext reverseCtx = { TranslateReverseGenericShareGenericInstFromDHE(originCtx.class_inst), TranslateReverseGenericShareGenericInstFromDHE(originCtx.method_inst) };
#if HYBRIDCLR_UNITY_2021_OR_NEW
		return il2cpp::vm::MetadataCache::GetGenericMethodPointers(aotMethod, &reverseCtx).methodPointer;
#else
		return il2cpp::vm::MetadataCache::GetMethodPointer(aotMethod, &reverseCtx, false, true);
#endif
	}

	Il2CppMethodPointer DifferentialHybridImage::TryGetAdjustThunkMethodPointer(const Il2CppMethodDefinition* method)
	{
		int32_t index = GetMethodDefinitionRawIndex(method);
		return _methodMappings[index].aotAdjustorThunk;
	}

	Il2CppMethodPointer DifferentialHybridImage::TryGetAdjustThunkMethodPointer(const MethodInfo* method)
	{
		if (method->is_generic)
		{
			return TryGetAdjustThunkMethodPointer((const Il2CppMethodDefinition*)method->methodMetadataHandle);
		}
		int32_t index = GetMethodDefinitionRawIndex((Il2CppMethodDefinition*)method->genericMethod->methodDefinition->methodMetadataHandle);
		MethodMapping& mm = _methodMappings[index];
		if (!mm.aotMethod)
		{
			return nullptr;
		}
		const MethodInfo* aotMethod = il2cpp::vm::GlobalMetadata::GetMethodInfoFromMethodHandle((Il2CppMetadataMethodDefinitionHandle)mm.aotMethod);
		const Il2CppGenericContext& originCtx = method->genericMethod->context;
		Il2CppGenericContext reverseCtx = { TranslateReverseGenericShareGenericInstFromDHE(originCtx.class_inst), TranslateReverseGenericShareGenericInstFromDHE(originCtx.method_inst) };
#if HYBRIDCLR_UNITY_2021_OR_NEW
		return il2cpp::vm::MetadataCache::GetGenericMethodPointers(aotMethod, &reverseCtx).virtualMethodPointer;
#else
		return il2cpp::vm::MetadataCache::GetMethodPointer(aotMethod, &reverseCtx, true, false);
#endif
	}

	InvokerMethod DifferentialHybridImage::TryGetMethodInvoker(const Il2CppMethodDefinition* method)
	{
		int32_t index = GetMethodDefinitionRawIndex(method);
		return _methodMappings[index].aotMethodInvoker;
	}

	InvokerMethod DifferentialHybridImage::TryGetMethodInvoker(const MethodInfo* method)
	{
		if (method->is_generic)
		{
			return TryGetMethodInvoker((const Il2CppMethodDefinition*)method->methodMetadataHandle);
		}
		int32_t index = GetMethodDefinitionRawIndex((Il2CppMethodDefinition*)method->genericMethod->methodDefinition->methodMetadataHandle);
		MethodMapping& mm = _methodMappings[index];
		if (!mm.aotMethod)
		{
			return nullptr;
		}
		const MethodInfo* aotMethod = il2cpp::vm::GlobalMetadata::GetMethodInfoFromMethodHandle((Il2CppMetadataMethodDefinitionHandle)mm.aotMethod);
		const Il2CppGenericContext& originCtx = method->genericMethod->context;
		Il2CppGenericContext reverseCtx = { TranslateReverseGenericShareGenericInstFromDHE(originCtx.class_inst), TranslateReverseGenericShareGenericInstFromDHE(originCtx.method_inst) };
#if HYBRIDCLR_UNITY_2021_OR_NEW
		return il2cpp::vm::MetadataCache::GetGenericMethodPointers(aotMethod, &reverseCtx).invoker_method;
#else
		return il2cpp::vm::MetadataCache::GetInvokerMethodPointer(aotMethod, &reverseCtx);
#endif
	}
}
}

#endif
