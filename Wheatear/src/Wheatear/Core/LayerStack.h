#pragma once

#include "Wheatear/Core/Core.h"

#include <memory>
#include <vector>

namespace Wheatear{

	class Layer;

	class WHEATEAR_API LayerStack
	{
	public:
		LayerStack();
		~LayerStack();
		
		void PushLayer(std::unique_ptr<Layer> layer);
		void PushOverlay(std::unique_ptr<Layer> overlay);
		void PopLayer(Layer* layer);
		void PopOverlay(Layer* layer);
		void Clear();
		
		// Iterates as Layer* so call sites keep the raw-pointer idiom; the stack
		// owns the layers and unique_ptr releases them on pop/clear.
		class iterator
		{
		public:
			using iterator_category = std::forward_iterator_tag;
			using value_type = Layer*;
			using difference_type = std::ptrdiff_t;
			using pointer = Layer**;
			using reference = Layer*;

			iterator() = default;
			explicit iterator(std::vector<std::unique_ptr<Layer>>::iterator it) : m_It(it) {}

			Layer* operator*() const { return m_It->get(); }
			iterator& operator++() { ++m_It; return *this; }
			iterator operator++(int) { iterator tmp = *this; ++m_It; return tmp; }
			bool operator==(const iterator& other) const { return m_It == other.m_It; }
			bool operator!=(const iterator& other) const { return m_It != other.m_It; }

		private:
			std::vector<std::unique_ptr<Layer>>::iterator m_It;
		};

		iterator begin() { return iterator(m_Layers.begin()); }
		iterator end() { return iterator(m_Layers.end()); }
	private:
		std::vector<std::unique_ptr<Layer>> m_Layers;
		unsigned int m_LayerInsertIndex = 0;
		};
}
