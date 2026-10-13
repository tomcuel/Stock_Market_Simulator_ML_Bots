import yfinance as yf
import pandas as pd
import numpy as np
import plotly.graph_objects as go
from keras.models import Sequential
from keras.layers import LSTM, Dense
from sklearn.preprocessing import MinMaxScaler
from sklearn.model_selection import train_test_split
import streamlit as st

model_cache = {}

def fetch_data(symbol):
    stock = yf.Ticker(symbol)
    data = stock.history(period="5y")
    return data[['Close']]

def preprocess_data(df):
    df['SMA_50'] = df['Close'].rolling(window=50).mean()
    df['EMA_50'] = df['Close'].ewm(span=20, adjust=False).mean()
    df['Daily_Return'] = df['Close'].pct_change()
    df['Log_Return'] = np.log(df['Close'] / df['Close'].shift(1))

    delta = df['Close'].diff(1)
    gain = (delta.where(delta >0,0)).rolling(window=14).mean()
    loss = (-delta.where(delta<0,0)).rolling(window=14).mean()
    rs = gain / loss
    df['RSI_14'] = 100 -(100 / (1 + rs))
    df['Middle_Band'] = df['Close'].rolling(window=20).mean()
    df['Upper_Band'] = df['Middle_Band'] + (df['Close'].rolling(window=20).std() * 2)
    df['Lower_Band'] = df['Middle_Band'] - (df['Close'].rolling(window=20).std() * 2)
    df['Momentum'] = df['Close'] - df['Close'].shift(4)
    df['Volatility'] = df['Close'].rolling(window=21).std()

    df.dropna(inplace=True)
    return df

def normalize_data(df):
    scaler = MinMaxScaler()
    scaled_data = scaler.fit_transform(df[['Close']])
    return scaled_data, scaler

def prepare_data(scaled_data, time_steps=60):
    X, y = [], []
    for i in range(time_steps, len(scaled_data)):
        X.append(scaled_data[i - time_steps:i])
        y.append(scaled_data[i,0])
    return np.array(X), np.array(y)

def build_model(input_shape):
    model = Sequential([
        LSTM(50, return_sequences=True, input_shape=input_shape),
        LSTM(50, return_sequences=False),
        Dense(25),
        Dense(1)
    ])
    model.compile(optimizer='adam', loss = 'mean_squared_error')
    print(model.summary())
    return model

st.title('Stock Price Prediction')

stock_list = ['AAPL', 'MSFT', 'GOOGL', 'TSLA', 'AMZN']
selected_stock = st.selectbox('Select the Stock:', stock_list)

st.write(f"Fetching data for {selected_stock}...")
data = fetch_data(selected_stock)
latest_price = data['Close'].iloc[-1]
st.write(f'Latest Price: ${latest_price:.2f}')

if st.button("Train and Predict"):
    st.write("Please Wait...")

    if selected_stock in model_cache:
        model, scaler = model_cache[selected_stock]
    else:
        data = preprocess_data(data)
        scaled_data, scaler = normalize_data(data)

        X, y = prepare_data(scaled_data)

        x_train, x_test, y_train, y_test = train_test_split(X,y, test_size=0.2, random_state=42)
        model = build_model((x_train.shape[1], x_train.shape[2]))
        model.fit(x_train, y_train, batch_size=32, epochs=5)

        loss = model.evaluate(x_test, y_test, verbose=0)
        st.write(f"Model Evaluation - MSE: {loss:.4f}")

        model_cache[selected_stock] = (model, scaler)

    st.write("Predicting prices for the next 100 days...")
    X_all, y_all = prepare_data(scaled_data)  
    y_pred_all = model.predict(X_all, verbose=0)
    y_pred_all = scaler.inverse_transform(y_pred_all)[:,0]
    prediction_dates = data.index[60:]
    in_sample_df = pd.DataFrame({'Date': prediction_dates,'Predicted': y_pred_all,'Actual': data['Close'][60:]})
    future_seq = scaled_data[-60:].reshape(60, 1)
    future_preds = []
    for day in range(100):
        X_input = future_seq.reshape(1, 60, 1)
        pred = model.predict(X_input, verbose=0)[0][0]
        future_preds.append(pred)
        future_seq = np.vstack([future_seq[1:], [[pred]]])
    future_preds = scaler.inverse_transform(np.array(future_preds).reshape(-1, 1))[:,0]
    future_dates = pd.date_range(start=pd.Timestamp.now() + pd.DateOffset(1), periods=100)

    # Plot everything together
    fig = go.Figure()

    # Historical Close Price
    fig.add_trace(go.Scatter(x=data.index, y=data['Close'], mode='lines', name='Historical Close', line=dict(color='lightgray')))
    # In-sample Predictions
    fig.add_trace(go.Scatter(x=in_sample_df['Date'], y=in_sample_df['Predicted'], mode='lines', name='Model Predictions', line=dict(color='blue')))
    # Future Forecasts
    fig.add_trace(go.Scatter(x=future_dates, y=future_preds, mode='lines+markers', name='Future Forecast', line=dict(color='red', dash='dot')))
    fig.update_layout(title='100-Day Stock Price Prediction', xaxis_title='Date', yaxis_title='Price (USD)', template='plotly_dark')
    
    st.write("Predicted Price:")
    st.table(in_sample_df.tail())
    st.plotly_chart(fig)
