﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExData.h"

#include "PCGExPointsMT.h"

namespace PCGExData
{
#pragma region Pools & cache

	void FBufferBase::SetTargetOutputName(const FName InName)
	{
		TargetOutputName = InName;
	}

	bool FBufferBase::OutputsToDifferentName() const
	{
		return false;
	}

	TSharedPtr<FBufferBase> FFacade::FindBuffer_Unsafe(const uint64 UID)
	{
		TSharedPtr<FBufferBase>* Found = BufferMap.Find(UID);
		if (!Found) { return nullptr; }
		return *Found;
	}

	TSharedPtr<FBufferBase> FFacade::FindBuffer(const uint64 UID)
	{
		FReadScopeLock ReadScopeLock(BufferLock);
		return FindBuffer_Unsafe(UID);
	}

	TSharedPtr<FBufferBase> FFacade::FindReadableAttributeBuffer(const FName InName)
	{
		FReadScopeLock ReadScopeLock(BufferLock);
		for (const TSharedPtr<FBufferBase>& Buffer : Buffers)
		{
			if (!Buffer->IsReadable()) { continue; }
			if (Buffer->InAttribute && Buffer->InAttribute->Name == InName) { return Buffer; }
		}
		return nullptr;
	}

	TSharedPtr<FBufferBase> FFacade::FindWritableAttributeBuffer(const FName InName)
	{
		FReadScopeLock ReadScopeLock(BufferLock);
		for (const TSharedPtr<FBufferBase>& Buffer : Buffers)
		{
			if (!Buffer->IsWritable()) { continue; }
			if (Buffer->FullName == InName) { return Buffer; }
		}
		return nullptr;
	}

	TSharedPtr<FBufferBase> FFacade::GetWritable(const EPCGMetadataTypes Type, const FPCGMetadataAttributeBase* InAttribute, EBufferInit Init)
	{
#define PCGEX_TYPED_WRITABLE(_TYPE, _ID, ...) case EPCGMetadataTypes::_ID: return GetWritable<_TYPE>(static_cast<const FPCGMetadataAttribute<_TYPE>*>(InAttribute), Init);
		switch (Type)
		{
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TYPED_WRITABLE)
		default: return nullptr;
		}
#undef PCGEX_TYPED_WRITABLE
	}

	TSharedPtr<FBufferBase> FFacade::GetWritable(const EPCGMetadataTypes Type, const FName InName, EBufferInit Init)
	{
#define PCGEX_TYPED_WRITABLE(_TYPE, _ID, ...) case EPCGMetadataTypes::_ID: return GetWritable<_TYPE>(InName, Init);
		switch (Type)
		{
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TYPED_WRITABLE)
		default: return nullptr;
		}
#undef PCGEX_TYPED_WRITABLE
	}

#pragma endregion

#pragma region FFacade

	void FFacade::MarkCurrentBuffersReadAsComplete()
	{
		for (const TSharedPtr<FBufferBase>& Buffer : Buffers)
		{
			if (!Buffer.IsValid() || !Buffer->IsReadable()) { continue; }
			Buffer->bReadComplete = true;
		}
	}

	void FFacade::Write(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const bool bEnsureValidKeys)
	{
		if (!AsyncManager || !AsyncManager->IsAvailable() || !Source->GetOut()) { return; }

		//UE_LOG(LogTemp, Warning, TEXT("{%lld} Facade -> Write"), AsyncManager->Context->GetInputSettings<UPCGSettings>()->UID)

		if (ValidateOutputsBeforeWriting())
		{
			if (bEnsureValidKeys) { Source->GetOutKeys(true); }

			{
				FWriteScopeLock WriteScopeLock(BufferLock);

				for (int i = 0; i < Buffers.Num(); i++)
				{
					const TSharedPtr<FBufferBase> Buffer = Buffers[i];
					if (!Buffer.IsValid() || !Buffer->IsWritable() || !Buffer->IsEnabled()) { continue; }
					WriteBuffer(AsyncManager, Buffer, false);
				}
			}
		}

		Flush();
	}

	int32 FFacade::WriteBuffersAsCallbacks(const TSharedPtr<PCGExMT::FTaskGroup>& TaskGroup)
	{
		// !!! Requires manual flush !!!

		if (!TaskGroup || !ValidateOutputsBeforeWriting())
		{
			Flush();
			return -1;
		}

		int32 WritableCount = 0;
		Source->GetOutKeys(true);

		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			for (int i = 0; i < Buffers.Num(); i++)
			{
				const TSharedPtr<FBufferBase> Buffer = Buffers[i];
				if (!Buffer.IsValid() || !Buffer->IsWritable() || !Buffer->IsEnabled()) { continue; }

				TaskGroup->AddSimpleCallback([BufferRef = Buffer]() { BufferRef->Write(); });
				WritableCount++;
			}
		}

		return WritableCount;
	}

	void FFacade::WriteBuffers(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, PCGExMT::FCompletionCallback&& Callback)
	{
		if (!ValidateOutputsBeforeWriting())
		{
			Flush();
			return;
		}

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, WriteBuffersWithCallback);
		WriteBuffersWithCallback->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE, Callback]()
			{
				PCGEX_ASYNC_THIS
				This->Flush();
				Callback();
			};

		if (const int32 WritableCount = WriteBuffersAsCallbacks(WriteBuffersWithCallback); WritableCount <= 0)
		{
			// -1 is fail so no callback
			if (WritableCount == 0) { Callback(); }
			return;
		}

		WriteBuffersWithCallback->StartSimpleCallbacks();
	}

	bool FFacade::ValidateOutputsBeforeWriting() const
	{
		FPCGExContext* Context = Source->GetContext();

		// TODO : First check that no writable attempts to write to the same output twice
		// TODO : Delete writables whose output that have

		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			TSet<FName> UniqueOutputs;
			//TSet<FName> DeprecatedOutputs;

			for (int i = 0; i < Buffers.Num(); i++)
			{
				const TSharedPtr<FBufferBase> Buffer = Buffers[i];
				if (!Buffer.IsValid() || !Buffer->IsWritable() || !Buffer->IsEnabled()) { continue; }

				PCGEx::FAttributeIdentity Identity = Buffer->GetTargetOutputIdentity();
				bool bAlreadySet = false;
				UniqueOutputs.Add(Identity.Name, &bAlreadySet);

				if (bAlreadySet)
				{
					PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Attribute \"{0}\" is used at target output at least twice by different sources."), FText::FromName(Identity.Name)));
					return false;
				}

				if (Buffer->OutputsToDifferentName())
				{
					// Get rid of new attributes that are being redirected at the time of writing
					// This is not elegant, but it's while waiting for a proper refactor

					if (Buffer->bIsNewOutput && Buffer->OutAttribute)
					{
						// Note : there will be a problem if an output an attribute to a different name a different type than the original one
						// i.e, From@A<double>, To@B<FVector>
						// Although this should never be an issue due to templating
						//DeprecatedOutputs.Add(Buffer->OutAttribute->Name);
						Source->DeleteAttribute(Buffer->OutAttribute->Name);
					}
				}
			}

			// Make sure we don't deprecate any output that will be written to ?
			// UniqueOutputs > DeprecatedOutputs
		}

		return true;
	}

#pragma endregion

	bool FReadableBufferConfig::Validate(FPCGExContext* InContext, const TSharedRef<FFacade>& InFacade) const
	{
		return true;
	}

	void FReadableBufferConfig::Fetch(const TSharedRef<FFacade>& InFacade, const PCGExMT::FScope& Scope) const
	{
		PCGEx::ExecuteWithRightType(
			Identity.UnderlyingType, [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				TSharedPtr<TBuffer<T>> Reader = nullptr;
				switch (Mode)
				{
				case EBufferPreloadType::RawAttribute:
					Reader = InFacade->GetScopedReadable<T>(Identity.Name);
					break;
				case EBufferPreloadType::BroadcastFromName:
					Reader = InFacade->GetScopedBroadcaster<T>(Identity.Name);
					break;
				case EBufferPreloadType::BroadcastFromSelector:
					Reader = InFacade->GetScopedBroadcaster<T>(Selector);
					break;
				}
				Reader->Fetch(Scope);
			});
	}

	void FReadableBufferConfig::Read(const TSharedRef<FFacade>& InFacade) const
	{
		PCGEx::ExecuteWithRightType(
			Identity.UnderlyingType, [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				TSharedPtr<TBuffer<T>> Reader = nullptr;
				switch (Mode)
				{
				case EBufferPreloadType::RawAttribute:
					Reader = InFacade->GetReadable<T>(Identity.Name);
					break;
				case EBufferPreloadType::BroadcastFromName:
					Reader = InFacade->GetBroadcaster<T>(Identity.Name);
					break;
				case EBufferPreloadType::BroadcastFromSelector:
					Reader = InFacade->GetBroadcaster<T>(Selector);
					break;
				}
			});
	}

	bool FFacadePreloader::Validate(FPCGExContext* InContext, const TSharedRef<FFacade>& InFacade) const
	{
		if (BufferConfigs.IsEmpty()) { return true; }
		for (const FReadableBufferConfig& Config : BufferConfigs) { if (!Config.Validate(InContext, InFacade)) { return false; } }
		return true;
	}

	void FFacadePreloader::Fetch(const TSharedRef<FFacade>& InFacade, const PCGExMT::FScope& Scope) const
	{
		for (const FReadableBufferConfig& ExistingConfig : BufferConfigs) { ExistingConfig.Fetch(InFacade, Scope); }
	}

	void FFacadePreloader::Read(const TSharedRef<FFacade>& InFacade, const int32 ConfigIndex) const
	{
		BufferConfigs[ConfigIndex].Read(InFacade);
	}

	void FFacadePreloader::StartLoading(
		const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager,
		const TSharedRef<FFacade>& InDataFacade,
		const TSharedPtr<PCGExMT::FAsyncMultiHandle>& InParentHandle)
	{
		InternalDataFacadePtr = InDataFacade;

		if (!IsEmpty())
		{
			if (!Validate(AsyncManager->GetContext(), InDataFacade))
			{
				InternalDataFacadePtr.Reset();
				OnLoadingEnd();
				return;
			}

			PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, PrefetchAttributesTask)
			PrefetchAttributesTask->SetParent(InParentHandle);

			PrefetchAttributesTask->OnCompleteCallback =
				[PCGEX_ASYNC_THIS_CAPTURE]()
				{
					PCGEX_ASYNC_THIS
					This->OnLoadingEnd();
				};

			if (InDataFacade->bSupportsScopedGet)
			{
				PrefetchAttributesTask->OnSubLoopStartCallback =
					[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
					{
						PCGEX_ASYNC_THIS
						if (const TSharedPtr<FFacade> InternalFacade = This->InternalDataFacadePtr.Pin())
						{
							This->Fetch(InternalFacade.ToSharedRef(), Scope);
						}
					};

				PrefetchAttributesTask->StartSubLoops(InDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
			}
			else
			{
				PrefetchAttributesTask->OnSubLoopStartCallback =
					[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
					{
						PCGEX_ASYNC_THIS
						if (const TSharedPtr<FFacade> InternalFacade = This->InternalDataFacadePtr.Pin())
						{
							This->Read(InternalFacade.ToSharedRef(), Scope.Start);
						}
					};

				PrefetchAttributesTask->StartSubLoops(Num(), 1);
			}
		}
		else
		{
			OnLoadingEnd();
		}
	}

	void FFacadePreloader::OnLoadingEnd() const
	{
		if (TSharedPtr<FFacade> InternalFacade = InternalDataFacadePtr.Pin()) { InternalFacade->MarkCurrentBuffersReadAsComplete(); }
		if (OnCompleteCallback) { OnCompleteCallback(); }
	}

#pragma region Union Data

	void FUnionData::ComputeWeights(
		const TArray<TSharedPtr<FFacade>>& Sources, const TMap<uint32, int32>& SourcesIdx, const FPCGPoint& Target,
		const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails, TArray<int32>& OutIOIdx, TArray<int32>& OutPointsIdx, TArray<double>& OutWeights) const
	{
		const int32 NumHashes = ItemHashSet.Num();

		OutPointsIdx.SetNumUninitialized(NumHashes);
		OutWeights.SetNumUninitialized(NumHashes);
		OutIOIdx.SetNumUninitialized(NumHashes);

		double TotalWeight = 0;
		int32 Index = 0;

		for (const uint64 Hash : ItemHashSet)
		{
			uint32 IOIndex;
			uint32 PtIndex;
			PCGEx::H64(Hash, IOIndex, PtIndex);

			const int32* IOIdx = SourcesIdx.Find(IOIndex);
			if (!IOIdx) { continue; }

			OutIOIdx[Index] = *IOIdx;
			OutPointsIdx[Index] = PtIndex;

			const double Weight = InDistanceDetails->GetDistSquared(Sources[*IOIdx]->Source->GetInPoint(PtIndex), Target);
			OutWeights[Index] = Weight;
			TotalWeight += Weight;

			Index++;
		}

		if (Index == 0) { return; }

		OutPointsIdx.SetNum(Index);
		OutWeights.SetNum(Index);
		OutIOIdx.SetNum(Index);

		if (Index == 1)
		{
			OutWeights[0] = 1;
			return;
		}

		if (TotalWeight == 0)
		{
			const double StaticWeight = 1 / static_cast<double>(ItemHashSet.Num());
			for (double& Weight : OutWeights) { Weight = StaticWeight; }
			return;
		}

		for (double& Weight : OutWeights) { Weight = 1 - (Weight / TotalWeight); }
	}

	uint64 FUnionData::Add(const int32 IOIndex, const int32 PointIndex)
	{
		const uint64 H = PCGEx::H64(IOIndex, PointIndex);

		{
			FWriteScopeLock WriteScopeLock(UnionLock);
			IOIndices.Add(IOIndex);
			ItemHashSet.Add(H);
		}

		return H;
	}

	void FUnionData::Add(const int32 IOIndex, const TArray<int32>& PointIndices)
	{
		FWriteScopeLock WriteScopeLock(UnionLock);

		IOIndices.Add(IOIndex);
		for (const int32 A : PointIndices) { ItemHashSet.Add(PCGEx::H64(IOIndex, A)); }
	}

	void FUnionMetadata::SetNum(const int32 InNum)
	{
		// To be used only with NewEntryAt / NewEntryAt_Unsafe
		Entries.Init(nullptr, InNum);
	}

	TSharedPtr<FUnionData> FUnionMetadata::NewEntry_Unsafe(const int32 IOIndex, const int32 ItemIndex)
	{
		TSharedPtr<FUnionData> NewUnionData = Entries.Add_GetRef(MakeShared<FUnionData>());
		NewUnionData->Add(IOIndex, ItemIndex);
		return NewUnionData;
	}

	TSharedPtr<FUnionData> FUnionMetadata::NewEntryAt_Unsafe(const int32 ItemIndex)
	{
		Entries[ItemIndex] = MakeShared<FUnionData>();
		return Entries[ItemIndex];
	}

	uint64 FUnionMetadata::Append(const int32 Index, const int32 IOIndex, const int32 ItemIndex)
	{
		return Entries[Index]->Add(IOIndex, ItemIndex);
	}

	bool FUnionMetadata::IOIndexOverlap(const int32 InIdx, const TSet<int32>& InIndices)
	{
		const TSet<int32> Overlap = Entries[InIdx]->IOIndices.Intersect(InIndices);
		return Overlap.Num() > 0;
	}

	TSharedPtr<FFacade> TryGetSingleFacade(FPCGExContext* InContext, const FName InputPinLabel, const bool bTransactional, const bool bThrowError)
	{
		TSharedPtr<FFacade> SingleFacade;
		if (const TSharedPtr<FPointIO> SingleIO = TryGetSingleInput(InContext, InputPinLabel, bTransactional, bThrowError))
		{
			SingleFacade = MakeShared<FFacade>(SingleIO.ToSharedRef());
		}

		return SingleFacade;
	}

	bool TryGetFacades(FPCGExContext* InContext, const FName InputPinLabel, TArray<TSharedPtr<FFacade>>& OutFacades, const bool bThrowError, const bool bIsTransactional)
	{
		TSharedPtr<FPointIOCollection> TargetsCollection = MakeShared<FPointIOCollection>(InContext, InputPinLabel, EIOInit::None, bIsTransactional);
		if (TargetsCollection->IsEmpty())
		{
			if (bThrowError) { PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FText::FromString(TEXT("Missing or zero-points '{0}' inputs")), FText::FromName(InputPinLabel))); }
			return false;
		}

		OutFacades.Reserve(OutFacades.Num() + TargetsCollection->Num());
		for (const TSharedPtr<FPointIO>& IO : TargetsCollection->Pairs) { OutFacades.Add(MakeShared<FFacade>(IO.ToSharedRef())); }

		return true;
	}


#pragma endregion
}
