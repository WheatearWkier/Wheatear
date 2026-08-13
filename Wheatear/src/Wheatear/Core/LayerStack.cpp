#include "wtpch.h"
#include "LayerStack.h"

#include "Layer.h"

#include <algorithm>

namespace Wheatear {
	
	LayerStack::LayerStack() 
	{
	}
	
	LayerStack::~LayerStack() 
	{
		Clear();
	}
	
	void LayerStack::PushLayer(std::unique_ptr<Layer> layer) 
	{
		m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, std::move(layer));
		m_LayerInsertIndex++;
	}
	
	void LayerStack::PushOverlay(std::unique_ptr<Layer> overlay) {
		m_Layers.emplace_back(std::move(overlay));
	}

	// LayerStack owns the layers: pop detaches and deletes. Callers must NOT
	// delete after popping; Clear() releases any remaining layers.
	void LayerStack::PopLayer(Layer* layer) {
		auto it = std::find_if(m_Layers.begin(), m_Layers.end(),
			[layer](const std::unique_ptr<Layer>& owned) { return owned.get() == layer; }); 
		if (it != m_Layers.end()) {
			layer->OnDetach();
			m_Layers.erase(it);
			m_LayerInsertIndex--;
		}
	}
		
	void LayerStack::PopOverlay(Layer* overlay) {
		auto it = std::find_if(m_Layers.begin(), m_Layers.end(),
			[overlay](const std::unique_ptr<Layer>& owned) { return owned.get() == overlay; });
		if (it != m_Layers.end()) {
			overlay->OnDetach();
			m_Layers.erase(it);
		}
	}

	void LayerStack::Clear()
	{
		for (const std::unique_ptr<Layer>& layer : m_Layers)
		{
			layer->OnDetach();
		}

		m_Layers.clear();
		m_LayerInsertIndex = 0;
	}

}
