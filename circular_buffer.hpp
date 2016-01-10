#ifndef circular_buffer_h
#define circular_buffer_h

#include <iostream>
#include <vector>
#include <numeric>
#include <type_traits>

namespace math
{
    
    template <typename T>
    class CircularBuffer
    {
    public:
        
        CircularBuffer()
        {
            bufferSize = 0;
            numValuesInBuffer = 0;
            readPtr = 0;
            writePtr = 0;
            bufferInit = false;
        }
        
        CircularBuffer(uint64_t bufferSize)
        {
            bufferInit = false;
            resize(bufferSize);
        }
        
        CircularBuffer(const CircularBuffer & rhs)
        {
            this->bufferSize = 0;
            this->numValuesInBuffer = 0;
            this->readPtr = 0;
            this->writePtr = 0;
            this->bufferInit = false;
            
            if(rhs.bufferInit)
            {
                this->bufferInit = rhs.bufferInit;
                this->bufferSize = rhs.bufferSize;
                this->numValuesInBuffer = rhs.numValuesInBuffer;
                this->buffer.resize(rhs.bufferSize);
                
                for(uint64_t i=0; i<rhs.bufferSize; i++)
                    this->buffer[i] = rhs.buffer[i];
                
                this->readPtr = rhs.readPtr;
                this->writePtr = rhs.writePtr;
            }
        }
        
        CircularBuffer & operator= (const CircularBuffer &rhs)
        {
            if (this != &rhs)
            {
                this->clear();
                if (rhs.bufferInit)
                {
                    this->bufferInit = rhs.bufferInit;
                    this->bufferSize = rhs.bufferSize;
                    this->numValuesInBuffer = rhs.numValuesInBuffer;
                    this->buffer.resize(rhs.bufferSize);
                    
                    for(uint64_t i=0; i<rhs.bufferSize; i++)
                        this->buffer[i] = rhs.buffer[i];
                    
                    this->readPtr = rhs.readPtr;
                    this->writePtr = rhs.writePtr;
                }
            }
            return *this;
        }
        
        virtual ~CircularBuffer()
        {
            if (bufferInit) clear();
        }
        
        inline T & operator[](const uint64_t & index)
        {
            return buffer[(readPtr + index) % bufferSize];
        }
        
        inline const T & operator[](const uint64_t & index) const
        {
            return buffer[(readPtr + index) % bufferSize];
        }
        
        inline T & operator()(const uint64_t &index)
        {
            return buffer[index];
        }
        
        inline const T & operator()(const uint64_t &index) const
        {
            return buffer[index];
        }
        
        bool resize(const uint64_t newBufferSize)
        {
            return resize(newBufferSize, T());
        }
        
        bool resize(const uint64_t newBufferSize, const T & defaultValue)
        {
            clear();
            
            if (newBufferSize == 0) return false;
            
            bufferSize = newBufferSize;
            buffer.resize(bufferSize, defaultValue);
            numValuesInBuffer = 0;
            readPtr = 0;
            writePtr = 0;
            
            bufferInit = true;
            
            return true;
        }
        
        bool put(const T & value)
        {
            if (!bufferInit) return false;
            
            // Add the value to the buffer
            buffer[writePtr] = value;
            
            // Update the write pointer
            writePtr++;
            writePtr = writePtr % bufferSize;
            
            // Check if the buffer is full
            if (++numValuesInBuffer > bufferSize)
            {
                numValuesInBuffer = bufferSize;
                //Only update the read pointer if the buffer has been filled
                readPtr++;
                readPtr = readPtr % bufferSize;
            }
            
            return true;
        }
        
        bool reinitialize_values(const T & value)
        {
            if (!bufferInit) return false;
            for (uint64_t i=0; i < bufferSize; i++)
                buffer[i] = value;
            return true;
        }
        
        void reset()
        {
            numValuesInBuffer = 0;
            readPtr = 0;
            writePtr = 0;
        }
        
        void clear()
        {
            numValuesInBuffer = 0;
            readPtr = 0;
            writePtr = 0;
            buffer.clear();
            bufferInit = false;
        }
        
        std::vector<T> get_data_as_vector() const
        {
            if (!bufferInit) throw std::runtime_error("buffer not initialized");
            std::vector<T> data(bufferSize);
            for (uint64_t i=0; i < bufferSize; i++)
                data[i] = (*this)[i];
            return data;
        }
        
        bool is_initialized() const { return bufferInit; }
        
        bool is_buffer_full() const { return bufferInit ? numValuesInBuffer == bufferSize : false; }
        
        uint64_t get_maximum_size() const { return bufferInit ? bufferSize : 0; }
        
        uint64_t get_current_size() const { return bufferInit ? numValuesInBuffer : 0; }
        
        uint64_t get_read_position() const { return bufferInit ? readPtr : 0; }
        
        uint64_t get_write_position() const { return bufferInit ? writePtr : 0; }
        
        T get_last(uint64_t samplesAgo) const
        {
            if (!bufferInit) throw std::runtime_error("buffer not initialized");
            return buffer[(readPtr - samplesAgo - 1 + get_current_size()) % get_current_size()];
        }
        
        // Numeric Analytics
        
        template<typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
        T compute_mean()
        {
            T sum = 0.0;
            for (int i = 0; i < get_current_size(); i++)
            {
                sum += buffer[i];
            }
            return T (sum / get_current_size());
        }
        
        template<typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
        T compute_variance()
        {
            T mean = compute_mean();
            T sum = 0;
            for (int i = 0; i < get_current_size(); i++)
            {
                sum += pow(buffer[i] - mean, 2);
            }
            return ((T) sqrt((sum / get_current_size())));
        }
        
        template<typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
        T compute_min()
        {
            T min = std::numeric_limits<T>::max();
            auto size = get_current_size();
            for (uint64_t i = 0; i < size; i++)
            {
                if (buffer[i] < min)
                    min = buffer[i];
            }
            return min;
        }
        
        template<typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
        T compute_max()
        {
            T max = std::numeric_limits<T>::min();
            auto size = get_current_size();
            for (uint64_t i = 0; i < size; i++)
            {
                if (buffer[i] > max)
                    max = buffer[i];
            }
            return max;
        }
        
    private:
        
        uint64_t bufferSize;
        uint64_t numValuesInBuffer;
        uint64_t readPtr;
        uint64_t writePtr;
        std::vector<T> buffer;
        bool bufferInit;
    };

} // end namespace math

#endif //circular_buffer_h
