// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExFactoryProvider.h"
#include "Data/PCGExPointFilter.h"


#include "Graph/PCGExCluster.h"


#include "PCGExClusterFilter.generated.h"

namespace PCGExGraph
{
	struct FEdge;
}

namespace PCGExPointFilter
{
	class FFilter;
}

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExClusterFilterFactoryData : public UPCGExFilterFactoryData
{
	GENERATED_BODY()

public:
	virtual bool SupportsCollectionEvaluation() const override { return false; }
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExNodeFilterFactoryData : public UPCGExClusterFilterFactoryData
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::FilterNode; }
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExEdgeFilterFactoryData : public UPCGExClusterFilterFactoryData
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::FilterEdge; }
};

namespace PCGExClusterFilter
{
	class PCGEXTENDEDTOOLKIT_API FFilter : public PCGExPointFilter::FFilter
	{
	public:
		explicit FFilter(const TObjectPtr<const UPCGExClusterFilterFactoryData>& InFactory):
			PCGExPointFilter::FFilter(InFactory)
		{
		}

		bool bInitForCluster = false;
		TSharedPtr<PCGExCluster::FCluster> Cluster;
		TSharedPtr<PCGExData::FFacade> EdgeDataFacade;

		virtual PCGExFilters::EType GetFilterType() const override;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;
		virtual bool Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade);
		virtual void PostInit() override;
	};

	class PCGEXTENDEDTOOLKIT_API FVtxFilter : public FFilter
	{
	public:
		explicit FVtxFilter(const TObjectPtr<const UPCGExClusterFilterFactoryData>& InFactory):
			FFilter(InFactory)
		{
		}

		virtual PCGExFilters::EType GetFilterType() const override { return PCGExFilters::EType::Node; }
		virtual bool Test(const int32 Index) const override final;
		virtual bool Test(const PCGExCluster::FNode& Node) const override;
		virtual bool Test(const PCGExGraph::FEdge& Edge) const override final;
	};

	class PCGEXTENDEDTOOLKIT_API FEdgeFilter : public FFilter
	{
	public:
		explicit FEdgeFilter(const TObjectPtr<const UPCGExClusterFilterFactoryData>& InFactory):
			FFilter(InFactory)
		{
		}

		virtual PCGExFilters::EType GetFilterType() const override { return PCGExFilters::EType::Edge; }
		virtual bool Test(const int32 Index) const override final;
		virtual bool Test(const PCGExCluster::FNode& Node) const override final;
		virtual bool Test(const PCGExGraph::FEdge& Edge) const override;
	};

	class PCGEXTENDEDTOOLKIT_API FManager : public PCGExPointFilter::FManager
	{
	public:
		FManager(const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade);

		TSharedRef<PCGExCluster::FCluster> Cluster;
		TSharedRef<PCGExData::FFacade> EdgeDataFacade;

		virtual ~FManager() override
		{
		}

	protected:
		virtual bool InitFilter(FPCGExContext* InContext, const TSharedPtr<PCGExPointFilter::FFilter>& Filter) override;
		virtual void InitCache() override;
	};
}
