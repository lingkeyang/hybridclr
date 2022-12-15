#pragma once
#include <unordered_set>
#include <unordered_map>

#include "InterpreterImage.h"

#if HYBRIDCLR_ENABLE_DHE

namespace hybridclr
{
namespace metadata
{


	struct DifferentialHybridOption
	{
		std::vector<uint32_t> notChangeMethodTokens;

		bool TryUnmarshal(BlobReader& reader, std::vector<uint32_t>& arr)
		{
			uint32_t count;
			if (!reader.TryRead32(count))
			{
				return false;
			}
			for (uint32_t i = 0; i < count; i++)
			{
				uint32_t token;
				if (!reader.TryRead32(token))
				{
					return false;
				}
				arr.push_back(token);
			}
			return true;
		}

		bool Unmarshal(BlobReader& reader)
		{
			uint32_t signature;
			if (!reader.TryRead32(signature) || signature != 0xABCDABCD)
			{
				return false;
			}

			if (!TryUnmarshal(reader, notChangeMethodTokens))
			{
				return false;
			}
			
			return reader.IsEmpty();
		}
	};

	struct TypeMapping
	{
		bool inited;
		bool notChanged;
		const char* name;
		const Il2CppTypeDefinition* interpType;
		const Il2CppTypeDefinition* aotType;
		std::vector<int32_t> aotFieldIndex2InterpIndex;
	};

	struct MethodMapping
	{
		const Il2CppMethodDefinition* interpMethod;
		const Il2CppMethodDefinition* aotMethod;
		Il2CppMethodPointer aotMethodPointer;
		Il2CppMethodPointer aotAdjustorThunk;
		InvokerMethod aotMethodInvoker;
	};

	class DifferentialHybridImage : public InterpreterImage
	{
	public:
		static const Il2CppType* TranslateIl2CppTypeToDHE(const Il2CppType* type);
		//static const Il2CppType* TranslateReverseGenericShareIl2CppTypeFromDHE(const Il2CppType* type);
		static const Il2CppGenericInst* TranslateGenericInstToDHE(const Il2CppGenericInst* gi);
		//static const Il2CppGenericInst* TranslateReverseGenericShareGenericInstFromDHE(const Il2CppGenericInst* gi);
		static const Il2CppMethodDefinition* TranslateMethodDefinitionToDHE(const Il2CppMethodDefinition* methodDef);
		static const Il2CppGenericMethod* TranslateGenericMethodToDHE(const Il2CppGenericMethod* methodDef);

	public:
		DifferentialHybridImage(uint32_t imageIndex, const DifferentialHybridOption& options) : InterpreterImage(imageIndex),
			_notChangeMethodTokens(options.notChangeMethodTokens.begin(), options.notChangeMethodTokens.end())
		{
		}

		uint32_t GetOriginTokenByInterpreterToken(uint32_t token)
		{
			auto it = _interpreterToken2OriginAOTTokens.find(token);
			return it != _interpreterToken2OriginAOTTokens.end() ? it->second : 0;
		}

		const Il2CppTypeDefinition* GetOriginType(const Il2CppTypeDefinition* type)
		{
			uint32_t index = GetTypeRawIndex(type);
			IL2CPP_ASSERT(index < (uint32_t)_typeMappings.size());
			return _typeMappings[index].aotType;
		}

		//const Il2CppTypeDefinition* GetGenericShareOriginType(const Il2CppTypeDefinition* type)
		//{
		//	uint32_t index = GetTypeRawIndex(type);
		//	IL2CPP_ASSERT(index < (uint32_t)_typeMappings.size());
		//	TypeMapping& tm = _typeMappings[index];
		//	return tm.notChanged ? tm.aotType : nullptr;
		//}

		const Il2CppTypeDefinition* GetInterpreterTypeByOriginType(const Il2CppTypeDefinition* type)
		{
			auto it = _originType2InterpreterTypes.find(type);
			return it != _originType2InterpreterTypes.end() ? it->second : nullptr;
		}

		const Il2CppMethodDefinition* GetInterpreterMethodDefinitionByOriginMethodDefinition(const Il2CppMethodDefinition* methodDef)
		{
			auto it = _originMethodToken2InterpreterMethodDefs.find(methodDef->token);
			return it != _originMethodToken2InterpreterMethodDefs.end() ? it->second : nullptr;
		}

		const Il2CppMethodDefinition* GetUnchangedOriginMethodDefintionByInterpreterMethod(const Il2CppMethodDefinition* methodDef)
		{
			int32_t index = GetMethodDefinitionRawIndex(methodDef);
			return _methodMappings[index].aotMethod;
		}

		FieldIndex GetInterpreterFieldIndexByOriginFieldIndex(const Il2CppTypeDefinition* interpType, FieldIndex index)
		{
			int32_t typeIndex = GetTypeRawIndex(interpType);
			TypeMapping& tm = _typeMappings[typeIndex];
			if (!tm.aotType)
			{
				return kFieldIndexInvalid;
			}
			IL2CPP_ASSERT((size_t)index < tm.aotFieldIndex2InterpIndex.size());
			return tm.aotFieldIndex2InterpIndex[index];
		}

		Il2CppMethodPointer TryGetMethodPointer(const Il2CppMethodDefinition* method);
		Il2CppMethodPointer TryGetMethodPointer(const MethodInfo* method);
		Il2CppMethodPointer TryGetAdjustThunkMethodPointer(const Il2CppMethodDefinition* method);
		Il2CppMethodPointer TryGetAdjustThunkMethodPointer(const MethodInfo* method);
		InvokerMethod TryGetMethodInvoker(const Il2CppMethodDefinition* method);
		InvokerMethod TryGetMethodInvoker(const MethodInfo* method);

		void BuildIl2CppImage(Il2CppImage* image, Il2CppAssembly* originAssembly);

		void InitRuntimeMetadatas() override;
	private:
		void InitTypeMapping(TypeMapping& type);
		void InitTypeMappings();
		void InitMethodMappings();
		void InitFieldMappings();

		const Il2CppAssembly* _originAssembly;
		const Il2CppImage* _originImage;
		std::unordered_set<uint32_t> _notChangeMethodTokens;
		std::unordered_map<uint32_t, const Il2CppMethodDefinition*> _originMethodToken2InterpreterMethodDefs;
		std::vector<TypeMapping> _typeMappings;
		std::unordered_map<const Il2CppTypeDefinition*, const Il2CppTypeDefinition*> _originType2InterpreterTypes;
		std::vector<MethodMapping> _methodMappings;
		std::unordered_map<uint32_t, uint32_t> _interpreterToken2OriginAOTTokens;
	};
}
}
#endif
